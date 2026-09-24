// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "MainComponent.h"
#include "WorkspaceGeometry.h"
#include "ChannelPeakMeter.h"
#include "LibraryWorkflow.h"
#include "SetRecorder.h"
#include "core/MasterPathProcessor.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

// Native post-mix wrapper: records the already-rendered master outputs 1/2,
// optionally mixes a selected microphone input with bounded ducking, applies
// a linked-stereo sample-peak safety limiter, and can mirror the protected
// master to a dedicated booth pair on logical outputs 5/6.
// MainComponent keeps ownership of playback/device logic; all disk I/O remains
// inside SetRecorder's background writer. Private cue stays on outputs 3/4.
class RecordingMainComponent final : public MainComponent {
public:
    explicit RecordingMainComponent(bool openAudio = true, bool enableKeyLockResearch = false)
        : MainComponent(openAudio, enableKeyLockResearch), libraryWorkflow(*this) {
        recordButton.setButtonText(text("REC SET", "NAGRAJ SET"));
        recordButton.setColour(juce::TextButton::buttonOnColourId,
                               BrokeLookAndFeel::recordRed().darker(0.10f));
        recordButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        recordButton.setTooltip(text(
            "Record post-limiter master outputs 1/2 to a 24-bit WAV. Disk encoding runs on a background thread; FIFO overflow is counted as a recording dropout instead of blocking playback.",
            "Nagraj wyjście master 1/2 po limiterze do WAV 24-bit. Zapis na dysku działa w tle; przepełnienie bufora jest liczone jako dropout nagrania zamiast blokować odtwarzanie."));
        recordButton.onClick = [this] { toggleRecording(); };
        addAndMakeVisible(recordButton);

        limiterButton.setButtonText("LIMIT");
        limiterButton.setClickingTogglesState(true);
        limiterButton.setToggleState(true, juce::dontSendNotification);
        limiterButton.setColour(juce::TextButton::buttonOnColourId,
                                BrokeLookAndFeel::accentDeep());
        limiterButton.setTooltip(text(
            "Linked-stereo -1 dBFS sample-peak safety limiter. Zero attack, 120 ms release, no look-ahead or true-peak reconstruction; not presented as a transparent mastering limiter.",
            "Sprzężony limiter stereo -1 dBFS dla szczytów próbek. Zerowy attack, release 120 ms, bez look-ahead i true-peak; nie jest przedstawiany jako przezroczysty limiter masteringowy."));
        limiterButton.onClick = [this] {
            masterPath.setLimiterEnabled(limiterButton.getToggleState());
        };
        addAndMakeVisible(limiterButton);

        micButton.setButtonText("MIC");
        micButton.setClickingTogglesState(true);
        micButton.setColour(juce::TextButton::buttonOnColourId,
                            BrokeLookAndFeel::warningAmber().darker(0.36f));
        micButton.setTooltip(text(
            "Mix the first active audio input into master 1/2 at 0 dB with up to 12 dB music ducking. Enable an input with MIC I/O first.",
            "Dodaj pierwsze aktywne wejście audio do master 1/2 przy 0 dB z duckingiem muzyki do 12 dB. Najpierw włącz wejście przez MIC I/O."));
        micButton.onClick = [this] {
            const bool requested = micButton.getToggleState();
            if (requested && !microphoneInputAvailable.load(std::memory_order_acquire)) {
                micButton.setToggleState(false, juce::dontSendNotification);
                masterPath.setMicrophoneEnabled(false);
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    text("Microphone input unavailable", "Wejście mikrofonowe niedostępne"),
                    text("Open MIC I/O, enable an input channel, then enable MIC again. BrokeDJ does not fold a missing input into the master path.",
                         "Otwórz MIC I/O, włącz kanał wejściowy, a potem ponownie włącz MIC. BrokeDJ nie dodaje brakującego wejścia do mastera."));
                return;
            }
            masterPath.setMicrophoneEnabled(requested);
        };
        addAndMakeVisible(micButton);

        boothButton.setButtonText("BOOTH");
        boothButton.setClickingTogglesState(true);
        boothButton.setColour(juce::TextButton::buttonOnColourId,
                              BrokeLookAndFeel::accentDeep().brighter(0.05f));
        boothButton.setTooltip(text(
            "Send the protected master to dedicated logical outputs 5/6. Requires six active output channels; cue remains private on 3/4.",
            "Wyślij zabezpieczony master na osobne wyjścia logiczne 5/6. Wymaga sześciu aktywnych wyjść; odsłuch pozostaje prywatny na 3/4."));
        boothButton.onClick = [this] {
            const bool requested = boothButton.getToggleState();
            if (requested && !boothOutputAvailable.load(std::memory_order_acquire)) {
                boothButton.setToggleState(false, juce::dontSendNotification);
                masterPath.setBoothEnabled(false);
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    text("Booth outputs unavailable", "Wyjścia booth niedostępne"),
                    text("Open MIC I/O and enable six output channels. Master uses 1/2, private cue uses 3/4 and Booth uses 5/6.",
                         "Otwórz MIC I/O i włącz sześć kanałów wyjściowych. Master używa 1/2, prywatny odsłuch 3/4, a Booth 5/6."));
                return;
            }
            masterPath.setBoothEnabled(requested);
        };
        addAndMakeVisible(boothButton);

        boothLevel.setSliderStyle(juce::Slider::LinearHorizontal);
        boothLevel.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 22);
        boothLevel.setRange(-60.0, 0.0, 0.1);
        boothLevel.setValue(-6.0, juce::dontSendNotification);
        boothLevel.setDoubleClickReturnValue(true, -6.0);
        boothLevel.setTextValueSuffix(" dB");
        boothLevel.setColour(juce::Slider::trackColourId,
                             BrokeLookAndFeel::signalGreen().withAlpha(0.78f));
        boothLevel.setColour(juce::Slider::thumbColourId,
                             BrokeLookAndFeel::signalGreen());
        boothLevel.setTooltip(text(
            "Independent Booth attenuation after the master limiter. Range -60..0 dB; it cannot boost above the protected master.",
            "Niezależne tłumienie Booth po limiterze master. Zakres -60..0 dB; nie może podbić sygnału ponad zabezpieczony master."));
        boothLevel.onValueChange = [this] {
            masterPath.setBoothGainDb(static_cast<float>(boothLevel.getValue()));
        };
        addAndMakeVisible(boothLevel);

        micIoButton.setButtonText("MIC I/O");
        micIoButton.setTooltip(text(
            "Select optional input channels and 2–6 output channels. Master uses 1/2, private cue 3/4 and optional Booth 5/6. The default launch still requests output only.",
            "Wybierz opcjonalne kanały wejściowe oraz 2–6 kanałów wyjściowych. Master używa 1/2, prywatny odsłuch 3/4, a opcjonalny Booth 5/6. Domyślnie program uruchamia tylko wyjście."));
        micIoButton.onClick = [this] { showMicIoSettings(); };
        addAndMakeVisible(micIoButton);

        masterPath.setMicrophoneGainDb(0.0f);
        masterPath.setDuckDepthDb(12.0f);
        masterPath.setLimiterCeilingDb(-1.0f);
        masterPath.setLimiterEnabled(true);
        masterPath.setBoothGainDb(-6.0f);
        masterPath.setBoothEnabled(false);

        initialiseWorkstationUi();
    }

    ~RecordingMainComponent() override {
        recordChooser.reset();
        if (micSettings) delete micSettings.getComponent();
        recorder.stop();
    }

    [[nodiscard]] bool usingWorkstationLayout() const noexcept { return workstationLayout; }

    void setDeckPreparationVisible(std::size_t deck, bool visible) {
        if (deck >= deckUi.size()) return;
        preparationVisible[deck] = visible;
        prepareButtons[deck].setToggleState(visible, juce::dontSendNotification);
        prepareButtons[deck].setButtonText(visible ? "MIX" : "GRID");
        if (workstationLayout) layoutDeckForWorkstation(deckUi[deck], deck);
    }

    [[nodiscard]] bool deckPreparationVisible(std::size_t deck) const noexcept {
        return deck < preparationVisible.size() && preparationVisible[deck];
    }


    [[nodiscard]] bool uiGeometrySane() const noexcept {
        if (!workstationUiReady) return false;
        const auto local = getLocalBounds();
        for (const auto& deck : deckUi)
            if (!local.contains(deck.deck->getBounds()) || deck.deck->getBounds().isEmpty()) return false;
        if (!workstationLayout) return true;
        if (mixerBounds.isEmpty() || !local.contains(mixerBounds)) return false;
        for (const auto& channel : channelUi) {
            for (const auto& control : channel.controls)
                if (control.slider->getParentComponent() != this || control.label->getParentComponent() != this
                    || control.slider->getBounds().isEmpty() || control.label->getBounds().isEmpty()
                    || !mixerBounds.contains(control.slider->getBounds())
                    || !mixerBounds.contains(control.label->getBounds())) return false;
        }
        for (std::size_t index = 0; index < deckUi.size(); ++index) {
            if (!channelMeters[index].isVisible() || channelMeters[index].getBounds().isEmpty()
                || !mixerBounds.contains(channelMeters[index].getBounds())
                || channelMeters[index].getBounds().intersects(channelUi[index].controls[1].slider->getBounds()))
                return false;
            const auto& deck = deckUi[index];
            if (deck.deck->getBounds().intersects(mixerBounds)) return false;
            const auto bounds = deck.deck->getLocalBounds();
            std::array<const juce::Component*, 40> visible{};
            std::size_t count = 0;
            const auto add = [&](const juce::Component* child) {
                if (!child->isVisible() || child->getParentComponent() != deck.deck) return true;
                if (child->getBounds().isEmpty() || !bounds.contains(child->getBounds())) return false;
                if (count >= visible.size()) return false;
                visible[count++] = child;
                return true;
            };
            if (!add(deck.waveform)) return false;
            for (auto* child : deck.buttons) if (!add(child)) return false;
            for (auto* child : deck.combos) if (!add(child)) return false;
            for (auto* child : deck.sliders) if (!add(child)) return false;
            for (auto* child : deck.labels) if (!add(child)) return false;
            for (std::size_t a = 0; a < count; ++a)
                for (std::size_t b = a + 1; b < count; ++b)
                    if (visible[a]->getBounds().intersects(visible[b]->getBounds())) return false;
            if (deck.buttons[gridResetButton]->isVisible() != preparationVisible[index]
                || deck.buttons[tempoMapButton]->isVisible() != preparationVisible[index]
                || deck.sliders[8]->isVisible() != preparationVisible[index]
                || deck.sliders[9]->isVisible() != preparationVisible[index]
                || deck.sliders[6]->isVisible() == preparationVisible[index]
                || deck.sliders[7]->isVisible() == preparationVisible[index]) return false;
            for (const juce::Component* child : std::array<const juce::Component*, 2>{
                     &deckHeadings[index], &prepareButtons[index]}) {
                if (!child->isVisible() || child->getBounds().isEmpty()
                    || !deck.deck->getBounds().contains(child->getBounds())) return false;
                for (std::size_t other = 0; other < count; ++other)
                    if (child->getBounds().intersects(visible[other]->getBounds().translated(
                        deck.deck->getX(), deck.deck->getY()))) return false;
            }
        }
        return true;
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override {
        MainComponent::prepareToPlay(samplesPerBlockExpected, sampleRate);
        preparedSampleRate.store(std::isfinite(sampleRate) ? sampleRate : 0.0,
                                 std::memory_order_release);
        masterPath.prepare(sampleRate);
        microphoneScratch.assign(static_cast<std::size_t>(std::max(1, samplesPerBlockExpected)), 0.0f);

        bool hasInput = false;
        bool hasBooth = false;
        if (auto* device = deviceManager.getCurrentAudioDevice()) {
            hasInput = device->getActiveInputChannels().countNumberOfSetBits() > 0;
            hasBooth = device->getActiveOutputChannels().countNumberOfSetBits() >= 6;
        }
        microphoneInputAvailable.store(hasInput, std::memory_order_release);
        boothOutputAvailable.store(hasBooth, std::memory_order_release);

        if (!hasInput && masterPath.isMicrophoneEnabled()) {
            masterPath.setMicrophoneEnabled(false);
            juce::MessageManager::callAsync([safe = juce::Component::SafePointer<RecordingMainComponent>(this)] {
                if (safe) safe->micButton.setToggleState(false, juce::dontSendNotification);
            });
        }
        if (!hasBooth && masterPath.isBoothEnabled()) {
            masterPath.setBoothEnabled(false);
            juce::MessageManager::callAsync([safe = juce::Component::SafePointer<RecordingMainComponent>(this)] {
                if (safe) safe->boothButton.setToggleState(false, juce::dontSendNotification);
            });
        }
    }

    void releaseResources() override {
        recorder.stop();
        preparedSampleRate.store(0.0, std::memory_order_release);
        microphoneInputAvailable.store(false, std::memory_order_release);
        boothOutputAvailable.store(false, std::memory_order_release);
        masterPath.setMicrophoneEnabled(false);
        masterPath.setBoothEnabled(false);
        masterPath.resetRealtimeState();
        MainComponent::releaseResources();
        juce::MessageManager::callAsync([safe = juce::Component::SafePointer<RecordingMainComponent>(this)] {
            if (!safe) return;
            safe->recordButton.setToggleState(false, juce::dontSendNotification);
            safe->recordButton.setButtonText(text("REC SET", "NAGRAJ SET"));
            safe->micButton.setToggleState(false, juce::dontSendNotification);
            safe->boothButton.setToggleState(false, juce::dontSendNotification);
        });
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override {
        const bool copyMic = info.buffer != nullptr && info.numSamples > 0
            && microphoneInputAvailable.load(std::memory_order_acquire)
            && masterPath.isMicrophoneEnabled()
            && info.buffer->getNumChannels() > 0
            && static_cast<std::size_t>(info.numSamples) <= microphoneScratch.size();
        if (copyMic) {
            const auto* input = info.buffer->getReadPointer(0, info.startSample);
            std::copy_n(input, info.numSamples, microphoneScratch.data());
        }

        MainComponent::getNextAudioBlock(info);
        if (info.buffer == nullptr || info.numSamples <= 0) return;

        // Engine owns master 1/2 and private cue 3/4. Dedicated channels above
        // those buses are cleared before optional Booth generation so stale
        // device/input samples can never leak to outputs 5/6 or beyond.
        for (int channel = 4; channel < info.buffer->getNumChannels(); ++channel)
            info.buffer->clear(channel, info.startSample, info.numSamples);

        if (info.buffer->getNumChannels() >= 2) {
            auto* left = info.buffer->getWritePointer(0, info.startSample);
            auto* right = info.buffer->getWritePointer(1, info.startSample);
            float* boothLeft = nullptr;
            float* boothRight = nullptr;
            if (info.buffer->getNumChannels() >= 6) {
                boothLeft = info.buffer->getWritePointer(4, info.startSample);
                boothRight = info.buffer->getWritePointer(5, info.startSample);
            }
            masterPath.process(copyMic ? microphoneScratch.data() : nullptr,
                               left, right, info.numSamples, boothLeft, boothRight);
        }

        if (!recorder.isRecording()) return;
        if (info.buffer->getNumChannels() < 2) {
            recorder.capture(nullptr, nullptr, info.numSamples);
            return;
        }
        recorder.capture(info.buffer->getReadPointer(0, info.startSample),
                         info.buffer->getReadPointer(1, info.startSample),
                         info.numSamples);
    }

    void paint(juce::Graphics& graphics) override {
        graphics.fillAll(BrokeLookAndFeel::background());

        auto toolbar = juce::Rectangle<float>(194.0f, 8.0f,
            static_cast<float>(std::max(1, getWidth() - 202)), 86.0f);
        graphics.setColour(BrokeLookAndFeel::surface());
        graphics.fillRoundedRectangle(toolbar, 10.0f);
        graphics.setColour(BrokeLookAndFeel::outline());
        graphics.drawRoundedRectangle(toolbar, 10.0f, 1.0f);

        if (workstationLayout && !mixerBounds.isEmpty()) {
            auto mixer = mixerBounds.toFloat();
            graphics.setColour(juce::Colours::black.withAlpha(0.42f));
            graphics.fillRoundedRectangle(mixer.translated(0.0f, 2.0f), 13.0f);
            juce::ColourGradient mixerGradient(BrokeLookAndFeel::surfaceHighlight().withAlpha(0.98f),
                                               mixer.getTopLeft(),
                                               BrokeLookAndFeel::background().interpolatedWith(
                                                   BrokeLookAndFeel::surface(), 0.72f),
                                               mixer.getBottomLeft(), false);
            graphics.setGradientFill(mixerGradient);
            graphics.fillRoundedRectangle(mixer, 13.0f);
            graphics.setColour(BrokeLookAndFeel::cyan().withAlpha(0.26f));
            graphics.drawRoundedRectangle(mixer.reduced(0.5f), 13.0f, 1.0f);

            auto inner = mixerBounds.reduced(9);
            const int stripWidth = std::max(1, inner.getWidth() / 4);
            for (int i = 1; i < 4; ++i) {
                const int x = inner.getX() + stripWidth * i;
                graphics.setColour(i == 2 ? BrokeLookAndFeel::cyan().withAlpha(0.28f)
                                          : BrokeLookAndFeel::outline().withAlpha(0.78f));
                graphics.drawVerticalLine(x, static_cast<float>(inner.getY() + 8),
                                           static_cast<float>(inner.getBottom() - 156));
            }
            auto masterShelf = mixerBounds.reduced(9).removeFromBottom(154).toFloat();
            graphics.setColour(BrokeLookAndFeel::background().withAlpha(0.52f));
            graphics.fillRoundedRectangle(masterShelf, 9.0f);
            graphics.setColour(BrokeLookAndFeel::outline().withAlpha(0.88f));
            graphics.drawRoundedRectangle(masterShelf, 9.0f, 1.0f);
        }
    }

    void resized() override {
        cacheWorkstationUi();
        const bool wantsWorkstation = workstationUiReady && getWidth() >= 1000 && getHeight() >= 720;
        setWorkstationLayoutEnabled(wantsWorkstation);

        MainComponent::resized();
        layoutTopActions();
        if (workstationLayout) layoutWorkstation();
    }

private:
    struct ControlPair {
        juce::Slider* slider = nullptr;
        juce::Label* label = nullptr;
    };

    static constexpr std::size_t deckButtonCount = 22;
    static constexpr std::size_t hotCueFirstButton = 12;
    static constexpr std::size_t hotCueButtonCount = 8;
    static constexpr std::size_t gridResetButton = 20;
    static constexpr std::size_t tempoMapButton = 21;

    using DeckUi = DeckPanel::WorkspaceControls;

    struct ChannelUi {
        DeckPanel* deck = nullptr;
        std::array<ControlPair, 5> controls{}; // trim, fader, low, mid, high
    };

    static juce::String dbfs(float linear) {
        if (!std::isfinite(linear) || linear <= 1.0e-9f) return "— dBFS";
        return juce::String(20.0f * std::log10(linear), 1) + " dBFS";
    }

    void initialiseWorkstationUi() {
        cacheWorkstationUi();
        constexpr std::array<const char*, 4> letters{"A", "B", "C", "D"};
        for (std::size_t deck = 0; deck < channelHeadings.size(); ++deck) {
            channelHeadings[deck].setText("CH " + juce::String(letters[deck]), juce::dontSendNotification);
            channelHeadings[deck].setJustificationType(juce::Justification::centred);
            channelHeadings[deck].setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
            channelHeadings[deck].setColour(juce::Label::textColourId,
                                            deck % 2 == 0 ? BrokeLookAndFeel::cyan()
                                                          : BrokeLookAndFeel::accentBlue().brighter(0.35f));
            channelHeadings[deck].setVisible(false);
            addAndMakeVisible(channelHeadings[deck]);
            channelHeadings[deck].setVisible(false);
            deckHeadings[deck].setText("DECK " + juce::String(letters[deck]), juce::dontSendNotification);
            deckHeadings[deck].setFont(juce::Font(juce::FontOptions(15.0f).withStyle("Bold")));
            deckHeadings[deck].setColour(juce::Label::textColourId,
                                        deck % 2 == 0 ? BrokeLookAndFeel::cyan()
                                                      : BrokeLookAndFeel::primaryText());
            addAndMakeVisible(deckHeadings[deck]);
            prepareButtons[deck].setButtonText("GRID");
            prepareButtons[deck].setName(text("Show grid editor for deck ", "Pokaż edytor siatki decku ")
                                         + juce::String(letters[deck]));
            prepareButtons[deck].setTooltip(text(
                "Switch between performance controls and grid preparation. Playback, rate, cues and mixer settings are preserved.",
                "Przełącz kontrolki miksowania i edycji siatki. Odtwarzanie, tempo, cue i ustawienia miksera zostają bez zmian."));
            prepareButtons[deck].setClickingTogglesState(true);
            prepareButtons[deck].onClick = [this, deck] {
                setDeckPreparationVisible(deck, prepareButtons[deck].getToggleState());
            };
            addAndMakeVisible(prepareButtons[deck]);
            channelMeters[deck].bind(*deckUi[deck].meter);
            channelMeters[deck].setName(text("Pre-fader peak, deck ", "Szczyt pre-fader, deck ")
                                       + juce::String(letters[deck]));
            channelMeters[deck].setTooltip(text(
                "Pre-fader sample peak: trim/EQ/FX are included, channel fader is not. Amber: -6 dBFS; red: 0 dBFS or overload. UI-sampled, not true peak.",
                "Szczyt próbek pre-fader: po trim/EQ/FX, przed faderem kanału. Bursztynowy: -6 dBFS; czerwony: 0 dBFS lub przester. Próbkowany przez UI, nie true peak."));
            addAndMakeVisible(channelMeters[deck]);
        }
    }

    void cacheWorkstationUi() {
        if (workstationUiCached) return;
        // Explicit handles, not child order or translated label matching.
        const auto controls = MainComponent::workspaceControls();
        deckUi = controls.decks;
        globalMixSliders = controls.mixSliders;
        crossfaderTitle = controls.mixLabels[0];
        masterTitle = controls.mixLabels[1];
        cueTitle = controls.mixLabels[2];
        masterMeter = controls.mixLabels[3];
        for (std::size_t deckIndex = 0; deckIndex < deckUi.size(); ++deckIndex) {
            auto& ui = deckUi[deckIndex];
            channelUi[deckIndex].deck = ui.deck;
            constexpr std::array<int, 5> indices{0, 1, 3, 4, 5};
            for (std::size_t control = 0; control < indices.size(); ++control) {
                const auto index = static_cast<std::size_t>(indices[control]);
                channelUi[deckIndex].controls[control] = {ui.sliders[index], ui.labels[4 + index]};
            }
            for (auto* slider : ui.sliders) {
                slider->setColour(juce::Slider::rotarySliderFillColourId, BrokeLookAndFeel::cyan());
                slider->setColour(juce::Slider::rotarySliderOutlineColourId, BrokeLookAndFeel::outline());
                slider->setColour(juce::Slider::thumbColourId, BrokeLookAndFeel::cyan());
                slider->setColour(juce::Slider::textBoxTextColourId, BrokeLookAndFeel::primaryText());
                slider->setColour(juce::Slider::textBoxOutlineColourId, BrokeLookAndFeel::outline());
            }
            for (auto* button : ui.buttons) {
                button->setColour(juce::TextButton::buttonColourId, BrokeLookAndFeel::surfaceRaised());
                button->setColour(juce::TextButton::buttonOnColourId, BrokeLookAndFeel::accentDeep());
            }
        }
        for (auto* slider : globalMixSliders) {
            slider->setColour(juce::Slider::trackColourId, BrokeLookAndFeel::accentBlue());
            slider->setColour(juce::Slider::thumbColourId, BrokeLookAndFeel::cyan());
            slider->setColour(juce::Slider::textBoxTextColourId, BrokeLookAndFeel::primaryText());
            slider->setColour(juce::Slider::textBoxOutlineColourId, BrokeLookAndFeel::outline());
        }
        workstationUiReady = true;
        workstationUiCached = true;
    }

    void setWorkstationLayoutEnabled(bool enabled) {
        if (!workstationUiReady || workstationLayout == enabled) return;
        workstationLayout = enabled;

        for (std::size_t deckIndex = 0; deckIndex < channelUi.size(); ++deckIndex) {
            auto& channel = channelUi[deckIndex];
            for (std::size_t controlIndex = 0; controlIndex < channel.controls.size(); ++controlIndex) {
                auto& control = channel.controls[controlIndex];
                auto* destination = enabled ? static_cast<juce::Component*>(this)
                                            : static_cast<juce::Component*>(channel.deck);
                destination->addAndMakeVisible(*control.slider);
                destination->addAndMakeVisible(*control.label);
                control.slider->setSliderStyle(controlIndex == 1 && enabled
                    ? juce::Slider::LinearVertical
                    : juce::Slider::RotaryHorizontalVerticalDrag);
                control.slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 18);
            }
            channelHeadings[deckIndex].setVisible(enabled);
            channelMeters[deckIndex].setVisible(enabled);
            deckHeadings[deckIndex].setVisible(enabled);
            prepareButtons[deckIndex].setVisible(enabled);
            auto& ui = deckUi[deckIndex];
            ui.labels[0]->setVisible(!enabled);
            if (!enabled) {
                for (auto* label : ui.labels) label->setVisible(true);
                for (auto* button : ui.buttons) button->setVisible(true);
                for (auto* slider : ui.sliders) slider->setVisible(true);
                ui.buttons[4]->setButtonText(text("Headphones", "Słuchawki"));
            }
        }
        mixerBounds = {};
        repaint();
    }

    static juce::Rectangle<int> nativeRect(broke::ui::Rect rect) noexcept {
        return {rect.x, rect.y, rect.width, rect.height};
    }

    void layoutTopActions() {
        const auto layout = broke::ui::workspaceGeometry(getWidth(), getHeight());
        const auto controls = MainComponent::workspaceControls();
        controls.headerLabels[0]->setBounds(nativeRect(layout.title));
        controls.headerLabels[1]->setBounds(nativeRect(layout.subtitle));
        controls.headerLabels[2]->setBounds(nativeRect(layout.status));
        controls.audioSettings->setBounds(nativeRect(layout.audioSettings));
        controls.author->setBounds(nativeRect(layout.author));
        recordButton.setBounds(nativeRect(layout.record));
        limiterButton.setBounds(nativeRect(layout.limiter));
        micButton.setBounds(nativeRect(layout.microphone));
        boothButton.setBounds(nativeRect(layout.booth));
        boothLevel.setBounds(nativeRect(layout.boothLevel));
        micIoButton.setBounds(nativeRect(layout.microphoneIo));
        libraryWorkflow.setButtonBounds(nativeRect(layout.library));
        libraryWorkflow.setSessionButtonBounds(nativeRect(layout.session));
    }

    static void layoutEqualRow(juce::Rectangle<int> area,
                               const std::initializer_list<juce::Component*>& components,
                               int inset = 2) {
        const int count = static_cast<int>(components.size());
        if (count <= 0) return;
        int remaining = count;
        for (auto* component : components) {
            const int width = remaining > 0 ? area.getWidth() / remaining : area.getWidth();
            auto slot = area.removeFromLeft(width);
            if (component != nullptr) component->setBounds(slot.reduced(inset, 0));
            --remaining;
        }
    }

    static void placeRotary(ControlPair& control, juce::Rectangle<int>& strip, int controlHeight) {
        if (control.slider == nullptr || control.label == nullptr) return;
        auto block = strip.removeFromTop(std::min(controlHeight, strip.getHeight()));
        control.label->setBounds(block.removeFromTop(15));
        control.slider->setBounds(block.reduced(3, 0));
        strip.removeFromTop(std::min(3, strip.getHeight()));
    }

    void layoutDeckForWorkstation(DeckUi& ui, std::size_t deckIndex) {
        auto area = ui.deck->getLocalBounds().reduced(10);
        const bool compact = ui.deck->getHeight() < 340;
        const bool preparing = preparationVisible[deckIndex];
        const auto inParent = [&ui](juce::Rectangle<int> bounds) {
            return bounds.translated(ui.deck->getX(), ui.deck->getY());
        };

        auto top = area.removeFromTop(24);
        prepareButtons[deckIndex].setBounds(inParent(top.removeFromRight(56).reduced(1, 0)));
        deckHeadings[deckIndex].setBounds(inParent(top.removeFromLeft(80)));
        ui.labels[0]->setVisible(false); // Dense engineering peak text remains in compact base only.
        ui.labels[2]->setBounds(top);
        auto trackRow = area.removeFromTop(24);
        ui.buttons[0]->setBounds(trackRow.removeFromRight(60).reduced(1, 1));
        ui.labels[1]->setBounds(trackRow);
        ui.labels[3]->setBounds(area.removeFromTop(18));
        area.removeFromTop(4);

        const int transportHeight = compact ? 28 : 34;
        const int padHeight = compact ? 28 : 34;
        const int performanceHeight = 28;
        const int toolsHeight = preparing ? 64 : (compact ? 38 : 50);
        const int fixedBelow = 4 + transportHeight + 4 + padHeight + 4
                             + performanceHeight + 4 + toolsHeight;
        ui.waveform->setBounds(area.removeFromTop(std::max(24, area.getHeight() - fixedBelow)));
        area.removeFromTop(4);
        auto transport = area.removeFromTop(transportHeight);
        layoutEqualRow(transport, {ui.buttons[1], ui.buttons[2], ui.buttons[3],
                                  ui.buttons[5], ui.combos[0], ui.buttons[4]});
        ui.buttons[4]->setButtonText("CUE");
        area.removeFromTop(4);
        auto pads = area.removeFromTop(padHeight);
        layoutEqualRow(pads, {ui.buttons[12], ui.buttons[13], ui.buttons[14], ui.buttons[15],
                             ui.buttons[16], ui.buttons[17], ui.buttons[18], ui.buttons[19]});
        area.removeFromTop(4);
        auto performance = area.removeFromTop(performanceHeight);
        layoutEqualRow(performance, {ui.buttons[6], ui.combos[1], ui.buttons[7],
                                     ui.buttons[8], ui.buttons[9], ui.buttons[10], ui.buttons[11]});
        area.removeFromTop(4);

        // Rate is always accessible. GRID reveals the real existing editor in
        // place of Echo/Drive, not on top of the waveform or mixer controls.
        auto tools = area.removeFromTop(toolsHeight);
        const auto placeTool = [](juce::Rectangle<int> bounds, juce::Label* label, juce::Slider* slider) {
            label->setBounds(bounds.removeFromTop(14));
            slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 20);
            slider->setBounds(bounds.reduced(2, 0));
        };
        auto rate = tools.removeFromLeft(tools.getWidth() / 3);
        placeTool(rate.withHeight(std::min(50, rate.getHeight())), ui.labels[6], ui.sliders[2]);
        for (std::size_t index : {std::size_t{6}, std::size_t{7}}) {
            ui.sliders[index]->setVisible(!preparing);
            ui.labels[4 + index]->setVisible(!preparing);
        }
        for (std::size_t index : {std::size_t{8}, std::size_t{9}}) {
            ui.sliders[index]->setVisible(preparing);
            ui.labels[4 + index]->setVisible(preparing);
        }
        ui.buttons[gridResetButton]->setVisible(preparing);
        ui.buttons[tempoMapButton]->setVisible(preparing);
        if (preparing) {
            auto actions = tools.removeFromBottom(26);
            layoutEqualRow(actions, {ui.buttons[tempoMapButton], ui.buttons[gridResetButton]});
            auto zero = tools.removeFromLeft(tools.getWidth() / 2);
            ui.labels[12]->setBounds(zero.removeFromTop(14));
            ui.labels[13]->setBounds(tools.removeFromTop(14));
            // Editable numeric boxes have an honest unit and never shrink into
            // an unusable one-pixel slider at the minimum window width.
            for (auto* slider : {ui.sliders[8], ui.sliders[9]}) {
                slider->setSliderStyle(juce::Slider::LinearHorizontal);
                slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 84, 20);
            }
            ui.sliders[8]->setBounds(zero.reduced(2, 0));
            ui.sliders[9]->setBounds(tools.reduced(2, 0));
        } else {
            auto echo = tools.removeFromLeft(tools.getWidth() / 2);
            placeTool(echo, ui.labels[10], ui.sliders[6]);
            placeTool(tools, ui.labels[11], ui.sliders[7]);
        }
        deckHeadings[deckIndex].toFront(false);
        prepareButtons[deckIndex].toFront(false);
    }

    void layoutCentralMixer() {
        auto inner = mixerBounds.reduced(9);
        auto masterShelf = inner.removeFromBottom(std::min(146, inner.getHeight() / 3));
        inner.removeFromBottom(std::min(6, inner.getHeight()));

        constexpr std::array<std::size_t, 4> order{0, 2, 1, 3};
        int remaining = static_cast<int>(order.size());
        for (const auto deckIndex : order) {
            const int width = remaining > 0 ? inner.getWidth() / remaining : inner.getWidth();
            auto strip = inner.removeFromLeft(width).reduced(3, 0);
            channelHeadings[deckIndex].setBounds(strip.removeFromTop(24));
            strip.removeFromTop(std::min(2, strip.getHeight()));

            auto& channel = channelUi[deckIndex];
            const int rotaryHeight = std::max(54, std::min(70, (strip.getHeight() - 126) / 4));
            placeRotary(channel.controls[0], strip, rotaryHeight); // trim
            placeRotary(channel.controls[4], strip, rotaryHeight); // high
            placeRotary(channel.controls[3], strip, rotaryHeight); // mid
            placeRotary(channel.controls[2], strip, rotaryHeight); // low

            auto& fader = channel.controls[1];
            fader.label->setBounds(strip.removeFromTop(std::min(15, strip.getHeight())));
            channelMeters[deckIndex].setBounds(strip.removeFromRight(12).reduced(0, 4));
            fader.slider->setBounds(strip.reduced(3, 1));
            --remaining;
        }

        masterMeter->setBounds(masterShelf.removeFromTop(22).reduced(4, 0));
        masterShelf.removeFromTop(std::min(2, masterShelf.getHeight()));
        auto levels = masterShelf.removeFromTop(std::min(50, masterShelf.getHeight()));
        auto masterArea = levels.removeFromLeft(levels.getWidth() / 2).reduced(4, 0);
        masterTitle->setBounds(masterArea.removeFromTop(16));
        globalMixSliders[1]->setBounds(masterArea);
        auto cueArea = levels.reduced(4, 0);
        cueTitle->setBounds(cueArea.removeFromTop(16));
        globalMixSliders[2]->setBounds(cueArea);
        masterShelf.removeFromTop(std::min(4, masterShelf.getHeight()));
        crossfaderTitle->setBounds(masterShelf.removeFromTop(std::min(18, masterShelf.getHeight())));
        globalMixSliders[0]->setBounds(masterShelf.reduced(5, 0));
    }

    void layoutWorkstation() {
        const auto layout = broke::ui::workspaceGeometry(getWidth(), getHeight());
        mixerBounds = nativeRect(layout.mixer);
        for (std::size_t deck = 0; deck < deckUi.size(); ++deck) {
            deckUi[deck].deck->setBounds(nativeRect(layout.decks[deck]));
            layoutDeckForWorkstation(deckUi[deck], deck);
        }
        layoutCentralMixer();
        repaint();
    }

    void showMicIoSettings() {
        if (micSettings) {
            micSettings->toFront(true);
            return;
        }
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = text("BrokeDJ / Microphone and outputs", "BrokeDJ / Mikrofon i wyjścia");
        options.dialogBackgroundColour = BrokeLookAndFeel::background();
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.content.setOwned(new juce::AudioDeviceSelectorComponent(
            deviceManager, 0, 2, 2, 6, false, false, true, false));
        options.content->setSize(620, 500);
        options.componentToCentreAround = this;
        micSettings = options.launchAsync();
    }

    void toggleRecording() {
        if (recorder.isRecording()) {
            stopRecordingAndReport();
            return;
        }

        const double rate = preparedSampleRate.load(std::memory_order_acquire);
        if (!std::isfinite(rate) || rate < 8000.0) {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                text("Recording unavailable", "Nagrywanie niedostępne"),
                text("Select a working audio device before recording a set.",
                     "Wybierz działające urządzenie audio przed nagrywaniem setu."));
            return;
        }

        if (recordChooser) return;
        const auto stamp = juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S");
        auto directory = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
        if (!directory.isDirectory())
            directory = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        const auto suggested = directory.getChildFile("BrokeDJ-set-" + stamp + ".wav");
        recordChooser = std::make_unique<juce::FileChooser>(
            text("Record BrokeDJ set", "Nagraj set BrokeDJ"), suggested, "*.wav");

        juce::Component::SafePointer<RecordingMainComponent> safe(this);
        recordChooser->launchAsync(
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                | juce::FileBrowserComponent::warnAboutOverwriting,
            [safe, rate](const juce::FileChooser& chooser) {
                const auto selected = chooser.getResult();
                if (!safe) return;
                safe->recordChooser.reset();
                if (selected == juce::File{}) return;
                safe->startRecording(selected, rate);
            });
    }

    void startRecording(juce::File selected, double rate) {
        if (selected.getFileExtension().toLowerCase() != ".wav")
            selected = selected.withFileExtension("wav");
        if (selected.exists()) selected = selected.getNonexistentSibling(false);

        masterPath.resetMetrics();
        if (!recorder.start(selected, rate)) {
            const auto state = recorder.snapshot();
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                text("Recording could not start", "Nie można rozpocząć nagrywania"),
                state.error.isNotEmpty() ? state.error
                                         : text("The WAV recording path could not be prepared.",
                                                "Nie udało się przygotować ścieżki nagrania WAV."));
            return;
        }
        const auto state = recorder.snapshot();
        recordButton.setToggleState(true, juce::dontSendNotification);
        recordButton.setButtonText(text("STOP REC", "STOP NAGR."));
        recordButton.setTooltip(text("Recording post-limiter master 1/2 to: ", "Nagrywanie master 1/2 po limiterze do: ")
                                + state.destination.getFullPathName());
    }

    void stopRecordingAndReport() {
        recorder.stop();
        const auto state = recorder.snapshot();
        const auto masterState = masterPath.snapshot();
        recordButton.setToggleState(false, juce::dontSendNotification);
        recordButton.setButtonText(text("REC SET", "NAGRAJ SET"));

        juce::String detail;
        if (state.finalized) {
            detail = text("Saved: ", "Zapisano: ") + state.destination.getFullPathName();
        } else if (state.recoveryFile.existsAsFile()) {
            detail = text("Finalization failed; recovery file kept: ",
                          "Finalizacja nie powiodła się; zachowano plik odzyskiwania: ")
                + state.recoveryFile.getFullPathName();
        } else {
            detail = state.error.isNotEmpty() ? state.error
                                              : text("Recording stopped without a finalized file.",
                                                     "Nagrywanie zatrzymano bez finalnego pliku.");
        }
        detail << text(" | written frames: ", " | zapisane klatki: ")
               << static_cast<juce::int64>(state.writtenFrames)
               << text(" | dropped frames: ", " | pominięte klatki: ")
               << static_cast<juce::int64>(state.droppedFrames)
               << text(" | dropout events: ", " | zdarzenia dropout: ")
               << static_cast<juce::int64>(state.dropoutEvents)
               << text(" | limiter max GR: ", " | limiter max GR: ")
               << juce::String(masterState.maxGainReductionDb, 1) << " dB"
               << text(" | max output: ", " | max wyjście: ") << dbfs(masterState.maxOutputPeak);
        if (masterState.microphoneEnabled)
            detail << text(" | microphone ducking active", " | ducking mikrofonu aktywny");
        if (masterState.boothEnabled)
            detail << text(" | booth max: ", " | booth max: ") << dbfs(masterState.maxBoothPeak);
        recordButton.setTooltip(detail);

        const auto icon = state.finalized && state.dropoutEvents == 0
            ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon;
        juce::AlertWindow::showMessageBoxAsync(
            icon,
            state.finalized
                ? text("Set recording finished", "Nagrywanie setu zakończone")
                : text("Set recording needs attention", "Nagranie setu wymaga uwagi"),
            detail);
    }

    SetRecorder recorder;
    broke::MasterPathProcessor masterPath;
    juce::TextButton recordButton;
    juce::TextButton limiterButton;
    juce::TextButton micButton;
    juce::TextButton boothButton;
    juce::Slider boothLevel;
    juce::TextButton micIoButton;
    std::unique_ptr<juce::FileChooser> recordChooser;
    juce::Component::SafePointer<juce::DialogWindow> micSettings;
    std::vector<float> microphoneScratch;
    std::atomic<double> preparedSampleRate{0.0};
    std::atomic<bool> microphoneInputAvailable{false};
    std::atomic<bool> boothOutputAvailable{false};

    LibraryWorkflow libraryWorkflow;

    std::array<DeckUi, 4> deckUi{};
    std::array<ChannelUi, 4> channelUi{};
    std::array<juce::Label, 4> channelHeadings{};
    std::array<ChannelPeakMeter, 4> channelMeters{};
    std::array<juce::Label, 4> deckHeadings{};
    std::array<juce::TextButton, 4> prepareButtons{};
    std::array<bool, 4> preparationVisible{};
    std::array<CrossfaderSlider*, 3> globalMixSliders{};
    juce::Label* crossfaderTitle = nullptr;
    juce::Label* masterTitle = nullptr;
    juce::Label* cueTitle = nullptr;
    juce::Label* masterMeter = nullptr;
    juce::Rectangle<int> mixerBounds;
    bool workstationUiCached = false;
    bool workstationUiReady = false;
    bool workstationLayout = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordingMainComponent)
};
