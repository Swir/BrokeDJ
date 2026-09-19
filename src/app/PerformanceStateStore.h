// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "core/PerformanceDeckOwner.h"

#include <cstddef>

#include <juce_core/juce_core.h>

// Persistent, source-identity-bound hotcue data. This is deliberately separate
// from the realtime deck owner: disk I/O belongs to the application adapter,
// never to Engine::process(). The payload stores derived cue positions only and
// never embeds the raw local source path or filename.
struct TrackHotCueSnapshot final {
    using Cue = broke::PerformanceDeckOwner::HotCue;
    static constexpr std::size_t cueCount = broke::PerformanceDeckOwner::hotCueCount;

    broke::PerformanceDeckOwner::HotCueBank cues{};
};

class TrackHotCueStore final {
public:
    explicit TrackHotCueStore(juce::File root = defaultRoot());

    [[nodiscard]] bool load(const juce::File& source, TrackHotCueSnapshot& snapshot) const;
    [[nodiscard]] bool store(const juce::File& source, const TrackHotCueSnapshot& snapshot) const;
    [[nodiscard]] bool erase(const juce::File& source) const;
    [[nodiscard]] const juce::File& rootDirectory() const noexcept { return root; }

    [[nodiscard]] static juce::File defaultRoot();

private:
    [[nodiscard]] juce::File stateFileFor(const juce::File& source) const;
    juce::File root;
};
