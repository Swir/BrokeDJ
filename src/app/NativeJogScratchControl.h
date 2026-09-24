// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <JuceHeader.h>
#include "ThemeManager.h"
#include "core/Engine.h"
#include "core/JogScratchController.h"
#include "core/PerformanceDeckOwner.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>

// A real circular performance surface rather than a generic rotary knob. The
// slider still owns JUCE keyboard/mouse interaction, while paint is presentation
// only and reads the currently applied accent-theme palette directly.
class BrokePlatterSlider final : public juce::Slider {
public:
    std::function<void()> onThemeMenu;

    void mouseDown(const juce::MouseEvent& event) override {
        if (event.mods.isPopupMenu()) {
            if (onThemeMenu) onThemeMenu();
            return;
        }
        juce::Slider::mouseDown(event);
    }

    void paint(juce::Graphics& graphics) override {
        auto area = getLocalBounds().toFloat().reduced(3.0f);
        const auto diameter = std::min(area.getWidth(), area.getHeight());
        if (diameter <= 12.0f) return;

        juce::Rectangle<float> platterBounds(0.0f, 0.0f, diameter, diameter);
        platterBounds.setCentre(area.getCentre());

        // Read the active palette directly instead of relying on inherited JUCE
        // Slider colour IDs. The first Windows theme witness exposed that the
        // custom-painted platter could otherwise retain Electric Blue while the
        // surrounding workstation had already changed to Ultraviolet or Ember.
        const auto theme = BrokeThemeManager::palette(BrokeThemeManager::currentTheme());
        const auto outline = theme.outline;
        const auto accent = theme.accent;
        const auto marker = theme.accentAlt;
        const auto face = theme.surfaceRaised;
        const auto alpha = isEnabled() ? 1.0f : 0.46f;

        graphics.setColour(juce::Colours::black.withAlpha(0.48f * alpha));
        graphics.fillEllipse(platterBounds.translated(0.0f, 2.0f));

        juce::ColourGradient outer(face.brighter(0.10f), platterBounds.getTopLeft(),
                                   face.darker(0.42f), platterBounds.getBottomRight(), false);
        graphics.setGradientFill(outer);
        graphics.fillEllipse(platterBounds);
        graphics.setColour(outline.withAlpha(0.98f * alpha));
        graphics.drawEllipse(platterBounds, 1.35f);

        // Concentric platter grooves make the control scan like a deck surface,
        // while remaining deliberately abstract rather than claiming vinyl emulation.
        for (int ring = 1; ring <= 4; ++ring) {
            const auto inset = diameter * (0.075f + static_cast<float>(ring) * 0.055f);
            const auto groove = platterBounds.reduced(inset);
            if (groove.getWidth() <= 6.0f) break;
            graphics.setColour((ring % 2 == 0 ? accent : outline)
                                   .withAlpha((ring % 2 == 0 ? 0.16f : 0.27f) * alpha));
            graphics.drawEllipse(groove, ring == 4 ? 1.15f : 0.85f);
        }

        const auto minimum = getMinimum();
        const auto maximum = getMaximum();
        const auto range = maximum - minimum;
        const auto proportion = range > 0.0
            ? std::clamp((getValue() - minimum) / range, 0.0, 1.0)
            : 0.5;
        const auto sweep = juce::MathConstants<float>::pi * 1.5f;
        const auto angle = -sweep * 0.5f + static_cast<float>(proportion) * sweep;
        const auto markerRadius = diameter * 0.385f;
        const auto centre = platterBounds.getCentre();
        const auto markerX = centre.x + std::sin(angle) * markerRadius;
        const auto markerY = centre.y - std::cos(angle) * markerRadius;

        graphics.setColour(accent.withAlpha(0.14f * alpha));
        graphics.drawEllipse(platterBounds.reduced(diameter * 0.035f), 3.0f);
        graphics.setColour(marker.withAlpha(0.98f * alpha));
        graphics.fillEllipse(markerX - 2.8f, markerY - 2.8f, 5.6f, 5.6f);

        juce::Rectangle<float> hub(0.0f, 0.0f, diameter * 0.30f, diameter * 0.30f);
        hub.setCentre(centre);
        juce::ColourGradient hubGradient(face.brighter(0.07f), hub.getTopLeft(),
                                         juce::Colours::black.withAlpha(0.90f),
                                         hub.getBottomRight(), false);
        graphics.setGradientFill(hubGradient);
        graphics.fillEllipse(hub);
        graphics.setColour(accent.withAlpha(0.84f * alpha));
        graphics.drawEllipse(hub, 1.2f);

        graphics.setColour(juce::Colours::white.withAlpha(0.86f * alpha));
        graphics.setFont(juce::Font(juce::FontOptions(std::max(8.0f, diameter * 0.105f))
                                         .withStyle("Bold")));
        graphics.drawText("JOG", hub.toNearestInt(), juce::Justification::centred, false);
    }
};

// Native platter surface for one deck. It owns message-thread interaction only;
// audio continues through the production Engine transport and its existing
// smoothing/de-click path. The compact circular surface reserves horizontal space
// beside the waveform instead of stealing vertical waveform height, so the jog
// control remains visible in the first-Beta workstation layout.
class NativeJogScratchControl final : public juce::Component,
                                      private juce::ComponentListener,
                                      private juce::Timer {
public:
    NativeJogScratchControl(juce::Component& deckHost,
                            juce::Component& waveformAnchor,
                            broke::Engine& targetEngine,
                            std::size_t deckIndex)
        : host(deckHost), anchor(waveformAnchor), engine(targetEngine), deck(deckIndex),
          performanceOwner(targetEngine, deckIndex),
          controller(targetEngine, performanceOwner, deckIndex) {
        state.setJustificationType(juce::Justification::centred);
        state.setColour(juce::Label::textColourId, juce::Colour{0xffdcecff});
        state.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
        state.setInterceptsMouseClicks(false, false);
        setState(State::ready);
        addAndMakeVisible(state);

        platter.setName(localText("Jog / Scratch / Theme", "Jog / Scratch / Motyw"));
        platter.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        platter.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        platter.setRange(-broke::JogScratchController::maxAudibleSpeed,
                          broke::JogScratchController::maxAudibleSpeed, 0.01);
        platter.setValue(0.0, juce::dontSendNotification);
        platter.setDoubleClickReturnValue(true, 0.0);
        platter.setScrollWheelEnabled(false);
        platter.setMouseDragSensitivity(180);
        platter.setTooltip(localText(
            "Platter: hold and drag for bounded scratch playback. Drag toward reverse/forward and release to restore the pre-touch transport. Right-click this platter to choose the BrokeDJ accent theme. Slip and Beat Loop own transport and block scratch fail-closed. This is not hardware-qualified vinyl emulation.",
            "Talerz: przytrzymaj i przeciągaj, aby scratchować w bezpiecznym zakresie; puszczenie przywraca transport sprzed dotknięcia. Kliknij talerz prawym przyciskiem, aby wybrać motyw akcentów BrokeDJ. Slip i Beat Loop mają pierwszeństwo i bezpiecznie blokują scratch. To nie jest jeszcze sprzętowo zweryfikowana emulacja winylu."));
        addAndMakeVisible(platter);

        platter.onThemeMenu = [this] { BrokeThemeManager::showMenu(platter); };
        platter.onDragStart = [this] { beginGesture(); };
        platter.onValueChange = [this] { applyVelocity(); };
        platter.onDragEnd = [this] { endGesture(); };

        if (deck == 0) {
            themeButton.setButtonText(localText("THEME", "MOTYW"));
            themeButton.setName(localText("BrokeDJ accent theme", "Motyw akcentów BrokeDJ"));
            themeButton.setTooltip(localText(
                "Choose the workstation accent theme. This changes presentation only; audio, transport and session state are untouched.",
                "Wybierz motyw akcentów stanowiska. Zmienia tylko wygląd; audio, transport i stan sesji pozostają bez zmian."));
            themeButton.onClick = [this] { BrokeThemeManager::showMenu(themeButton); };
            addAndMakeVisible(themeButton);
        }

        host.addAndMakeVisible(*this);
        anchor.addComponentListener(this);
        layoutFromAnchor();

        if (deck == 0) {
            juce::Component::SafePointer<juce::Component> safeHost(&host);
            juce::MessageManager::callAsync([safeHost] {
                if (safeHost == nullptr) return;
                auto* root = safeHost->getTopLevelComponent();
                if (root == nullptr) root = safeHost.getComponent();
                if (root != nullptr) BrokeThemeManager::applyPersisted(*root);
            });
        }

        startTimerHz(20);
    }

    NativeJogScratchControl(const NativeJogScratchControl&) = delete;
    NativeJogScratchControl& operator=(const NativeJogScratchControl&) = delete;

    ~NativeJogScratchControl() override {
        stopTimer();
        anchor.removeComponentListener(this);
        static_cast<void>(controller.cancel());
    }

    [[nodiscard]] bool active() const noexcept { return gestureActive; }

    // Explicit fail-safe hook for future MIDI/device/clip lifecycle owners.
    void cancel() noexcept {
        if (gestureActive) static_cast<void>(controller.cancel());
        gestureActive = false;
        lastVelocity = 0.0;
        platter.setValue(0.0, juce::dontSendNotification);
        refreshIdleState();
    }

    void resized() override {
        auto area = getLocalBounds();
        auto header = area.removeFromTop(std::min(17, area.getHeight()));
        if (deck == 0) {
            themeButton.setBounds(header.removeFromRight(std::min(46, header.getWidth())).reduced(1, 0));
            state.setBounds(header);
        } else {
            state.setBounds(header);
        }
        auto wheelArea = area.reduced(2, 1);
        const int diameter = std::max(1, std::min(wheelArea.getWidth(), wheelArea.getHeight()));
        platter.setBounds(juce::Rectangle<int>(diameter, diameter).withCentre(wheelArea.getCentre()));
    }

private:
    enum class State { ready, active, blocked, unavailable };

    static juce::String localText(const char* english, const char* polish) {
        const bool usePolish = juce::SystemStats::getUserLanguage().startsWithIgnoreCase("pl");
        return juce::String::fromUTF8(usePolish ? polish : english);
    }

    void setState(State next) {
        switch (next) {
            case State::ready:
                state.setText(localText("READY", "GOTOWY"), juce::dontSendNotification);
                state.setColour(juce::Label::textColourId, juce::Colour{0xff8199b8});
                break;
            case State::active:
                state.setText(localText("ACTIVE", "AKTYW"), juce::dontSendNotification);
                state.setColour(juce::Label::textColourId, juce::Colour{0xff62e5ff});
                break;
            case State::blocked:
                state.setText(localText("LOCKED", "BLOK"), juce::dontSendNotification);
                state.setColour(juce::Label::textColourId, juce::Colour{0xffffc56b});
                break;
            case State::unavailable:
                state.setText(localText("EMPTY", "PUSTY"), juce::dontSendNotification);
                state.setColour(juce::Label::textColourId, juce::Colour{0xff8199b8});
                break;
        }
    }

    void refreshIdleState() {
        const double duration = engine.meter(deck).duration.load(std::memory_order_acquire);
        if (!std::isfinite(duration) || duration <= 0.0) {
            setState(State::unavailable);
            return;
        }
        const bool foreignTransport = performanceOwner.beatLoopActive()
            || engine.loopRegionEnabled(deck)
            || performanceOwner.reverseSlipMode() != broke::PerformanceDeckOwner::ReverseSlipMode::forward;
        setState(foreignTransport ? State::blocked : State::ready);
    }

    void beginGesture() {
        platter.setValue(0.0, juce::dontSendNotification);
        const auto result = controller.begin();
        if (result == broke::JogScratchController::Result::applied) {
            gestureActive = true;
            lastVelocity = 0.0;
            gestureDuration = engine.meter(deck).duration.load(std::memory_order_acquire);
            setState(State::active);
            return;
        }
        gestureActive = false;
        setState(result == broke::JogScratchController::Result::trackUnavailable
                     ? State::unavailable : State::blocked);
    }

    void applyVelocity() {
        if (!gestureActive) {
            if (platter.getValue() != 0.0)
                platter.setValue(0.0, juce::dontSendNotification);
            return;
        }
        double velocity = platter.getValue();
        if (std::abs(velocity) > broke::JogScratchController::stopDeadzone
            && std::abs(velocity) < broke::JogScratchController::minAudibleSpeed) {
            velocity = std::copysign(broke::JogScratchController::minAudibleSpeed, velocity);
            platter.setValue(velocity, juce::dontSendNotification);
        }
        const auto result = controller.setVelocity(velocity);
        if (result != broke::JogScratchController::Result::applied) {
            abortGesture(result == broke::JogScratchController::Result::trackUnavailable
                             ? State::unavailable : State::blocked);
            return;
        }
        lastVelocity = velocity;
        setState(State::active);
    }

    void endGesture() {
        if (!gestureActive) {
            platter.setValue(0.0, juce::dontSendNotification);
            refreshIdleState();
            return;
        }
        const auto result = controller.end();
        gestureActive = false;
        lastVelocity = 0.0;
        platter.setValue(0.0, juce::dontSendNotification);
        if (result == broke::JogScratchController::Result::applied)
            refreshIdleState();
        else
            setState(State::blocked);
    }

    void abortGesture(State terminalState) noexcept {
        static_cast<void>(controller.cancel());
        gestureActive = false;
        lastVelocity = 0.0;
        platter.setValue(0.0, juce::dontSendNotification);
        setState(terminalState);
    }

    void timerCallback() override {
        if (!gestureActive) {
            refreshIdleState();
            return;
        }

        // Clip replacement normally changes the published duration. Fail closed
        // rather than letting a held platter resume transport on a new source.
        const double duration = engine.meter(deck).duration.load(std::memory_order_acquire);
        const bool durationChanged = !std::isfinite(duration) || duration <= 0.0
            || !std::isfinite(gestureDuration)
            || std::abs(duration - gestureDuration) > 1.0e-6;
        const bool foreignTransport = engine.loopRegionEnabled(deck) || performanceOwner.slipEnabled();
        const bool reverseRejected = lastVelocity < -broke::JogScratchController::stopDeadzone
            && !performanceOwner.reverseEnabled();
        if (durationChanged || foreignTransport || reverseRejected)
            abortGesture(durationChanged ? State::unavailable : State::blocked);
    }

    void layoutFromAnchor() {
        if (adjustingLayout) return;
        const juce::ScopedValueSetter<bool> guard(adjustingLayout, true);
        auto waveformBounds = anchor.getBounds();
        constexpr int controlWidth = 94;
        constexpr int minimumWaveformWidth = 170;
        constexpr int minimumControlHeight = 50;
        if (waveformBounds.getHeight() < minimumControlHeight
            || waveformBounds.getWidth() < minimumWaveformWidth + controlWidth) {
            setVisible(false);
            return;
        }

        setVisible(true);
        auto platterBounds = waveformBounds.removeFromRight(controlWidth);
        waveformBounds.removeFromRight(std::min(4, waveformBounds.getWidth()));
        anchor.setBounds(waveformBounds);
        setBounds(platterBounds.reduced(1, 0));
    }

    void componentMovedOrResized(juce::Component& component, bool, bool wasResized) override {
        if (&component == &anchor && wasResized) layoutFromAnchor();
    }

    juce::Component& host;
    juce::Component& anchor;
    broke::Engine& engine;
    std::size_t deck = 0;
    broke::PerformanceDeckOwner performanceOwner;
    broke::JogScratchController controller;
    juce::Label state;
    BrokePlatterSlider platter;
    juce::TextButton themeButton;
    bool gestureActive = false;
    bool adjustingLayout = false;
    double gestureDuration = 0.0;
    double lastVelocity = 0.0;
};
