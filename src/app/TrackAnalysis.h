// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "core/BeatAnalysis.h"

#include <atomic>
#include <juce_audio_formats/juce_audio_formats.h>

struct TrackRhythmAnalysis final {
    broke::BeatAnalysisResult beat;
    juce::String error;
    bool cacheHit = false;
};

struct TrackAnalysisOptions final {
    broke::BeatAnalysisOptions beat;
    int readBlockFrames = 65536;
};

// Persistent analysis cache. Cache records intentionally contain only source
// size/timestamp plus derived musical metadata; local path/file names are not
// copied into the payload. The cache is never read or written by the audio
// callback.
class TrackAnalysisCache final {
public:
    explicit TrackAnalysisCache(juce::File root = defaultRoot());

    [[nodiscard]] bool load(const juce::File& source,
                            broke::BeatAnalysisResult& result) const;
    [[nodiscard]] bool store(const juce::File& source,
                             const broke::BeatAnalysisResult& result) const;
    [[nodiscard]] const juce::File& rootDirectory() const noexcept { return root; }

    [[nodiscard]] static juce::File defaultRoot();

private:
    [[nodiscard]] juce::File cacheFileFor(const juce::File& source) const;
    juce::File root;
};

// Sequential worker-only analysis. Decoding and cache I/O are deliberately
// separate from playback and the realtime callback; cancellation is checked
// between bounded read blocks.
TrackRhythmAnalysis analyzeTrackRhythm(const juce::File& file,
                                       const std::atomic<bool>& cancelled,
                                       const TrackAnalysisOptions& options = {});

TrackRhythmAnalysis analyzeTrackRhythmCached(const juce::File& file,
                                             const std::atomic<bool>& cancelled,
                                             const TrackAnalysisCache& cache,
                                             const TrackAnalysisOptions& options = {});
