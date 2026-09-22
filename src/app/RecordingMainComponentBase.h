// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "MainComponent.h"
#include "LibraryWorkflow.h"
#include "SetRecorder.h"
#include "core/MasterPathProcessor.h"

#include <algorithm>
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
        recordButton.setTooltip(text(
            "Record post-limiter master outputs 1/2 to a 24-bit WAV. Disk encoding runs on a background thread; FIFO overflow is counted as a recording dropout instead of blocking playback.",
            "Nagraj wyjście master 1/2 po limiterze do WAV 24-bit. Zapis na dysku działa w tle; przepełnienie bufora jest liczone jako dropout nagrania zamiast blokować odtwarzanie."));
        recordButton.onClick = [this] { toggleRecording(); };
        addAndMakeVisible(recordButton);

        limiterButton.setButtonText("LIMIT");
        limiterButton.setClickingTogglesState(true);
        limiterButton.setToggleState(true, juce::dontSendNotification);
        limiterButton.setTooltip(text(
            "Linked-stereo -1 dBFS sample-peak safety limiter. Zero attack, 120 ms release, no look-ahead or true-peak reconstruction; not presented as a transparent mastering limiter.",
            "Sprzężony limiter stereo -1 dBFS dla szczytów próbek. Zerowy attack, release 120 ms, bez look-ahead i true-peak; nie jest przedstawiany jako przezroczysty limiter masteringowy."));
        limiterButton.onClick = [this] {
            masterPath.setLimiterEnabled(limiterButton.getToggleState());
        };
        addAndMakeVisible(limiterButton);

        micButton.setButtonText("MIC");
        micButton.setClickingTogglesState(true);
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
    }

    ~RecordingMainComponent() override {
        recordChooser.reset();
        if (micSettings) delete micSettings.getComponent();
        recorder.stop();
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
        MainComponent::paint(graphics);

        // Visually group the operational controls into one instrument-like strip
        // without changing their ownership, hit targets or realtime behavior.
        const int toolbarLeft = std::max(164, getWidth() - 900);
        auto toolbar = juce::Rectangle<float>(static_cast<float>(toolbarLeft), 12.0f,
                                              static_cast<float>(std::max(1, getWidth() - toolbarLeft - 14)),
                                              50.0f);
        graphics.setColour(BrokeLookAndFeel::surface().withAlpha(0.94f));
        graphics.fillRoundedRectangle(toolbar, 9.0f);
        graphics.setColour(BrokeLookAndFeel::cyan().withAlpha(0.14f));
        graphics.drawRoundedRectangle(toolbar, 9.0f, 1.0f);
    }

    void resized() override {
        MainComponent::resized();
        constexpr int settingsWidth = 165;
        constexpr int gap = 6;
        int right = getWidth() - 20 - settingsWidth - 8;

        constexpr int recordWidth = 92;
        recordButton.setBounds(right - recordWidth, 20, recordWidth, 34);
        right -= recordWidth + gap;

        constexpr int limiterWidth = 58;
        limiterButton.setBounds(right - limiterWidth, 20, limiterWidth, 34);
        right -= limiterWidth + gap;

        constexpr int micWidth = 52;
        micButton.setBounds(right - micWidth, 20, micWidth, 34);
        right -= micWidth + gap;

        constexpr int boothWidth = 62;
        boothButton.setBounds(right - boothWidth, 20, boothWidth, 34);
        right -= boothWidth + gap;

        constexpr int boothLevelWidth = 130;
        boothLevel.setBounds(right - boothLevelWidth, 20, boothLevelWidth, 34);
        right -= boothLevelWidth + gap;

        constexpr int ioWidth = 64;
        micIoButton.setBounds(right - ioWidth, 20, ioWidth, 34);
        right -= ioWidth + gap;

        constexpr int libraryWidth = 82;
        libraryWorkflow.setButtonBounds({right - libraryWidth, 20, libraryWidth, 34});
    }

private:
    static juce::String dbfs(float linear) {
        if (!std::isfinite(linear) || linear <= 1.0e-9f) return "— dBFS";
        return juce::String(20.0f * std::log10(linear), 1) + " dBFS";
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
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordingMainComponent)
};