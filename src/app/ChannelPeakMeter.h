// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "BrokeLookAndFeel.h"
#include "core/Engine.h"
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>

// A message-thread, sampled pre-fader peak display, not a true-peak meter or
// loudness analyzer. The source is owned by MainComponent and outlives this UI.
class ChannelPeakMeter final : public juce::Component,
                               public juce::SettableTooltipClient,
                               private juce::Timer {
public:
    void bind(const broke::Meter& meter) {
        source = &meter;
        startTimerHz(25);
    }
    ~ChannelPeakMeter() override { stopTimer(); }
    void paint(juce::Graphics& graphics) override {
        auto area = getLocalBounds().toFloat().reduced(1.0f);
        const auto cap = area.removeFromTop(5.0f);
        graphics.setColour(overloaded ? BrokeLookAndFeel::recordRed()
                                     : BrokeLookAndFeel::outline());
        graphics.fillRoundedRectangle(cap, 1.5f);
        area.removeFromTop(3.0f);
        constexpr int segments = 18;
        const float segmentHeight = area.getHeight() / static_cast<float>(segments);
        for (int index = 0; index < segments; ++index) {
            const float threshold = -48.0f + static_cast<float>(index) * 3.0f;
            const auto colour = threshold >= 0.0f ? BrokeLookAndFeel::recordRed()
                              : threshold >= -6.0f ? BrokeLookAndFeel::warningAmber()
                                                    : BrokeLookAndFeel::cyan();
            graphics.setColour(db > threshold ? colour : BrokeLookAndFeel::outline().withAlpha(0.55f));
            graphics.fillRoundedRectangle(area.getX(),
                area.getBottom() - static_cast<float>(index + 1) * segmentHeight,
                area.getWidth(), std::max(1.0f, segmentHeight - 2.0f), 1.0f);
        }
    }
private:
    void timerCallback() override {
        if (source == nullptr) return;
        const float peak = source->preFaderPeak.load(std::memory_order_relaxed);
        const float measured = std::isfinite(peak) && peak > 1.0e-9f
            ? std::clamp(20.0f * std::log10(peak), -48.0f, 6.0f) : -48.0f;
        const float nextDb = std::max(measured, db - 0.8f);
        const bool nextOverload = source->overloaded.load(std::memory_order_relaxed);
        if (std::abs(nextDb - db) < 0.01f && nextOverload == overloaded) return;
        db = nextDb;
        overloaded = nextOverload;
        repaint();
    }
    const broke::Meter* source = nullptr;
    float db = -48.0f;
    bool overloaded = false;
};
