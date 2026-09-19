// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "core/BeatAnalysis.h"

#include <juce_core/juce_core.h>

// User-authored beat-grid corrections live separately from detector cache data.
// Records are tied to the current source-file identity (size + modification time),
// contain no raw path/name in their payload and are never touched by the audio
// callback. This keeps manual authority durable without pretending the detector
// produced edited metadata.
class BeatGridStore final {
public:
    explicit BeatGridStore(juce::File root = defaultRoot());

    [[nodiscard]] bool load(const juce::File& source, broke::BeatGrid& grid) const;
    [[nodiscard]] bool store(const juce::File& source, const broke::BeatGrid& grid) const;
    [[nodiscard]] bool erase(const juce::File& source) const;

    [[nodiscard]] const juce::File& rootDirectory() const noexcept { return root; }
    [[nodiscard]] static juce::File defaultRoot();

private:
    [[nodiscard]] juce::File gridFileFor(const juce::File& source) const;

    juce::File root;
};
