// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "BrokeLookAndFeel.h"
#include "core/Engine.h"
#include "core/MeterBallistics.h"
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>

// A message-thread, sampled pre-fader peak display, not a true-peak meter or
// loudness analyzer. The source is owned by MainComponent and outlives this UI.
// Ballistics are JUCE-independent and never run in Engine::process().
class ChannelPeakMeter final : public juce::Component,
                               public juce::SettableTooltipClient,
                               private juce::Timer {
public:
    ChannelPeakMeter() {
        setName(localText("Pre-fader peak meter", "Miernik szczytowy przed faderem"));
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        refreshTooltip();
    }

    void bind(const broke::Meter& meter) {
        source = &meter;
        ballistics.reset();
        refreshTooltip();
        startTimerHz(sampleRateHz);
    }

    ~ChannelPeakMeter() override { stopTimer(); }

    void paint(juce::Graphics& graphics) override {
        const auto snapshot = ballistics.snapshot();
        auto area = getLocalBounds().toFloat().reduced(1.0f);
        if (area.isEmpty()) return;

        const auto cap = area.removeFromTop(6.0f);
        graphics.setColour(snapshot.overloadLatched ? BrokeLookAndFeel::recordRed()
                                                    : BrokeLookAndFeel::outline());
        graphics.fillRoundedRectangle(cap, 1.5f);
        if (snapshot.overloadLatched) {
            graphics.setColour(juce::Colours::white.withAlpha(0.86f));
            graphics.drawHorizontalLine(static_cast<int>(std::round(cap.getCentreY())),
                                        cap.getX() + 1.0f, cap.getRight() - 1.0f);
        }

        area.removeFromTop(3.0f);
        constexpr int segments = 22; // -60 .. +6 dBFS in 3 dB bands
        const float segmentHeight = area.getHeight() / static_cast<float>(segments);
        for (int index = 0; index < segments; ++index) {
            const float threshold = broke::MeterBallistics::floorDb
                + static_cast<float>(index) * 3.0f;
            const auto colour = threshold >= 0.0f ? BrokeLookAndFeel::recordRed()
                              : threshold >= -6.0f ? BrokeLookAndFeel::warningAmber()
                                                    : BrokeLookAndFeel::cyan();
            const bool active = snapshot.displayDb >= threshold;
            graphics.setColour(active ? colour
                                      : BrokeLookAndFeel::outline().withAlpha(0.48f));
            graphics.fillRoundedRectangle(
                area.getX(),
                area.getBottom() - static_cast<float>(index + 1) * segmentHeight,
                area.getWidth(), std::max(1.0f, segmentHeight - 1.5f), 1.0f);
        }

        // Peak-hold is intentionally a sampled UI marker. It makes transient
        // gain-staging easier to read without claiming true-peak measurement.
        if (snapshot.holdDb > broke::MeterBallistics::floorDb + 0.5f) {
            const float proportion = broke::MeterBallistics::normalisedDb(snapshot.holdDb);
            const float y = area.getBottom() - proportion * area.getHeight();
            graphics.setColour(juce::Colours::white.withAlpha(0.90f));
            graphics.drawHorizontalLine(static_cast<int>(std::round(y)),
                                        area.getX(), area.getRight(), 1.35f);
        }
    }

    void mouseDown(const juce::MouseEvent& event) override {
        if (event.mods.isLeftButtonDown()) {
            ballistics.clearOverloadLatch();
            refreshTooltip();
            repaint();
        }
    }

private:
    static constexpr int sampleRateHz = 25;

    static juce::String localText(const char* english, const char* polish) {
        const bool usePolish = juce::SystemStats::getUserLanguage().startsWithIgnoreCase("pl");
        return juce::String::fromUTF8(usePolish ? polish : english);
    }

    static juce::String dbText(float db) {
        return db <= broke::MeterBallistics::floorDb + 0.01f
            ? juce::String("≤ ") + juce::String(broke::MeterBallistics::floorDb, 0) + " dBFS"
            : juce::String(db, 1) + " dBFS";
    }

    void refreshTooltip() {
        const auto snapshot = ballistics.snapshot();
        juce::String tooltip = localText(
            "Sampled pre-fader peak meter — not true-peak or loudness. Level: ",
            "Próbkowany miernik szczytowy przed faderem — nie true-peak ani loudness. Poziom: ");
        tooltip << dbText(snapshot.displayDb)
                << localText(" · hold: ", " · hold: ") << dbText(snapshot.holdDb);
        if (snapshot.overloadLatched)
            tooltip << localText(" · OVERLOAD latched. Left-click to clear.",
                                 " · OVERLOAD zapamiętany. Kliknij lewym, aby wyzerować.");
        else
            tooltip << localText(" · left-click clears the overload latch.",
                                 " · lewy klik zeruje pamięć przesterowania.");
        setTooltip(tooltip);
    }

    void timerCallback() override {
        if (source == nullptr) return;
        const auto before = ballistics.snapshot();
        const float peak = source->preFaderPeak.load(std::memory_order_relaxed);
        const bool overloaded = source->overloaded.load(std::memory_order_relaxed);
        ballistics.pushLinear(peak, overloaded, broke::MeterBallistics::defaultSampleSeconds);
        const auto after = ballistics.snapshot();

        const bool changed = std::abs(after.displayDb - before.displayDb) >= 0.05f
            || std::abs(after.holdDb - before.holdDb) >= 0.05f
            || after.overloadLatched != before.overloadLatched;
        if (!changed) return;
        refreshTooltip();
        repaint();
    }

    const broke::Meter* source = nullptr;
    broke::MeterBallistics ballistics;
};
