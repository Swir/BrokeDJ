// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <JuceHeader.h>
#include "core/Engine.h"
#include "core/JogScratchController.h"
#include "core/PerformanceDeckOwner.h"

#include <cmath>
#include <cstddef>

// Compact native platter strip for one deck. It deliberately owns only the
// message-thread interaction surface; audio continues through the production
// Engine transport and its existing smoothing/de-click path.
//
// The control is self-positioning relative to the deck waveform so it can be
// composed into the existing DeckPanel without duplicating layout policy. A
// dedicated PerformanceDeckOwner facade reads/writes the same authoritative
// Engine atomics and loop-region state as the rest of the deck. The underlying
// JogScratchController remains the single policy for bounded scratch ownership.
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
        state.setJustificationType(juce::Justification::centredLeft);
        state.setColour(juce::Label::textColourId, juce::Colour{0xffdcecff});
        state.setFont(juce::Font(juce::FontOptions(10.5f).withStyle("Bold")));
        setState(State::ready);
        addAndMakeVisible(state);

        platter.setName(localText("Jog / Scratch", "Jog / Scratch"));
        platter.setSliderStyle(juce::Slider::LinearHorizontal);
        platter.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 20);
        platter.setRange(-broke::JogScratchController::maxAudibleSpeed,
                          broke::JogScratchController::maxAudibleSpeed, 0.01);
        platter.setValue(0.0, juce::dontSendNotification);
        platter.setNumDecimalPlacesToDisplay(2);
        platter.setDoubleClickReturnValue(true, 0.0);
        platter.setTooltip(localText(
            "Hold and drag for bounded platter playback: left = reverse, centre = stopped, right = forward. Release restores the pre-touch transport. Slip and Beat Loop own transport and block scratch fail-closed. This is not hardware-qualified vinyl emulation.",
            "Przytrzymaj i przeciągaj: lewo = wstecz, środek = stop, prawo = do przodu. Puszczenie przywraca transport sprzed dotknięcia. Slip i Beat Loop mają pierwszeństwo i bezpiecznie blokują scratch. To nie jest jeszcze sprzętowo zweryfikowana emulacja winylu."));
        addAndMakeVisible(platter);

        platter.onDragStart = [this] { beginGesture(); };
        platter.onValueChange = [this] { applyVelocity(); };
        platter.onDragEnd = [this] { endGesture(); };

        host.addAndMakeVisible(*this);
        host.addComponentListener(this);
        startTimerHz(20);
    }

    NativeJogScratchControl(const NativeJogScratchControl&) = delete;
    NativeJogScratchControl& operator=(const NativeJogScratchControl&) = delete;

    ~NativeJogScratchControl() override {
        stopTimer();
        host.removeComponentListener(this);
        static_cast<void>(controller.cancel());
    }

    [[nodiscard]] bool active() const noexcept { return gestureActive; }

    // Explicit fail-safe hook for future MIDI/device/clip lifecycle owners.
    void cancel() noexcept {
        if (gestureActive) static_cast<void>(controller.cancel());
        gestureActive = false;
        lastVelocity = 0.0;
        platter.setValue(0.0, juce::dontSendNotification);
        setState(State::ready);
    }

    void resized() override {
        auto area = getLocalBounds();
        state.setBounds(area.removeFromLeft(104));
        platter.setBounds(area.reduced(2, 1));
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
                state.setText(localText("JOG READY", "JOG GOTOWY"), juce::dontSendNotification);
                state.setColour(juce::Label::textColourId, juce::Colour{0xff8199b8});
                break;
            case State::active:
                state.setText(localText("JOG ACTIVE", "JOG AKTYWNY"), juce::dontSendNotification);
                state.setColour(juce::Label::textColourId, juce::Colour{0xff62e5ff});
                break;
            case State::blocked:
                state.setText(localText("JOG BLOCKED", "JOG ZAJĘTY"), juce::dontSendNotification);
                state.setColour(juce::Label::textColourId, juce::Colour{0xffffc56b});
                break;
            case State::unavailable:
                state.setText(localText("NO TRACK", "BRAK UTW."), juce::dontSendNotification);
                state.setColour(juce::Label::textColourId, juce::Colour{0xff8199b8});
                break;
        }
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
        if (!gestureActive) return;
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
            return;
        }
        const auto result = controller.end();
        gestureActive = false;
        lastVelocity = 0.0;
        platter.setValue(0.0, juce::dontSendNotification);
        setState(result == broke::JogScratchController::Result::applied ? State::ready : State::blocked);
    }

    void abortGesture(State terminalState) noexcept {
        static_cast<void>(controller.cancel());
        gestureActive = false;
        lastVelocity = 0.0;
        platter.setValue(0.0, juce::dontSendNotification);
        setState(terminalState);
    }

    void timerCallback() override {
        if (!gestureActive) return;

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

    void componentMovedOrResized(juce::Component& component, bool, bool wasResized) override {
        if (&component != &host || !wasResized) return;
        auto waveformBounds = anchor.getBounds();
        constexpr int stripHeight = 34;
        constexpr int minimumWaveformHeight = 38;
        if (waveformBounds.getHeight() <= stripHeight + minimumWaveformHeight) {
            setVisible(false);
            return;
        }
        setVisible(true);
        auto strip = waveformBounds.removeFromBottom(stripHeight);
        anchor.setBounds(waveformBounds);
        setBounds(strip.reduced(1, 2));
    }

    juce::Component& host;
    juce::Component& anchor;
    broke::Engine& engine;
    std::size_t deck = 0;
    broke::PerformanceDeckOwner performanceOwner;
    broke::JogScratchController controller;
    juce::Label state;
    juce::Slider platter;
    bool gestureActive = false;
    double gestureDuration = 0.0;
    double lastVelocity = 0.0;
};
