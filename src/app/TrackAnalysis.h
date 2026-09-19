// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "core/BeatAnalysis.h"
#include "core/KeyAnalysis.h"

#include <atomic>
#include <utility>
#include <juce_audio_formats/juce_audio_formats.h>

struct TrackRhythmAnalysis final {
    broke::BeatAnalysisResult beat;
    broke::MusicalKeyResult key;
    juce::String error;
    juce::String beatError;
    juce::String keyError;
    bool cacheHit = false;
};

struct TrackAnalysisOptions final {
    broke::BeatAnalysisOptions beat;
    broke::KeyAnalysisOptions key;
    int readBlockFrames = 65536;
};

// Persistent detector-result cache. Cache records intentionally contain only
// source size/timestamp plus derived musical metadata; local path/file names are
// not copied into the payload. The cache is never read or written by the audio
// callback. Beat and key results are independently optional so a strong tonal
// result can be cached even when the tempo detector correctly declines a track.
class TrackAnalysisCache final {
public:
    explicit TrackAnalysisCache(juce::File root = defaultRoot());

    [[nodiscard]] bool load(const juce::File& source,
                            broke::BeatAnalysisResult& beat,
                            broke::MusicalKeyResult& key) const;
    [[nodiscard]] bool store(const juce::File& source,
                             const broke::BeatAnalysisResult& beat,
                             const broke::MusicalKeyResult& key) const;
    [[nodiscard]] const juce::File& rootDirectory() const noexcept { return root; }

    [[nodiscard]] static juce::File defaultRoot();

private:
    [[nodiscard]] juce::File cacheFileFor(const juce::File& source) const;
    juce::File root;
};

// User-authored beat-grid corrections are deliberately stored separately from
// detector output. Records are tied to the current source-file identity, omit
// raw local paths/names from their payload and are only accessed off callback.
// A changed source file invalidates the override instead of silently applying an
// old grid to different audio.
class TrackBeatGridOverrideStore final {
public:
    explicit TrackBeatGridOverrideStore(juce::File root = defaultRoot());

    [[nodiscard]] bool load(const juce::File& source, broke::BeatGrid& grid) const;
    [[nodiscard]] bool store(const juce::File& source, const broke::BeatGrid& grid) const;
    [[nodiscard]] bool erase(const juce::File& source) const;
    [[nodiscard]] const juce::File& rootDirectory() const noexcept { return root; }

    [[nodiscard]] static juce::File defaultRoot();

private:
    [[nodiscard]] juce::File overrideFileFor(const juce::File& source) const;
    juce::File root;
};

struct TrackBeatGridSelection final {
    broke::BeatGrid grid;
    bool manualOverride = false;
};

// Worker-only resolution contract used by the native app: a valid user override
// wins over detector output for the exact current source identity. If no valid
// override exists, the detected grid is used when valid. No file I/O belongs in
// the audio callback.
[[nodiscard]] inline TrackBeatGridSelection selectTrackBeatGrid(
    const juce::File& source,
    const broke::BeatAnalysisResult& detected,
    const TrackBeatGridOverrideStore& overrides) {
    TrackBeatGridSelection selection;
    broke::BeatGrid manual;
    if (overrides.load(source, manual) && manual.valid()) {
        selection.grid = std::move(manual);
        selection.manualOverride = true;
        return selection;
    }
    selection.grid = broke::BeatGrid(detected);
    return selection;
}

// Sequential worker-only analysis. Decoding and cache I/O are deliberately
// separate from playback and the realtime callback; cancellation is checked
// between bounded read blocks. Beat and key accumulators consume the same decode
// pass so musical metadata does not double file I/O.
TrackRhythmAnalysis analyzeTrackRhythm(const juce::File& file,
                                       const std::atomic<bool>& cancelled,
                                       const TrackAnalysisOptions& options = {});

TrackRhythmAnalysis analyzeTrackRhythmCached(const juce::File& file,
                                             const std::atomic<bool>& cancelled,
                                             const TrackAnalysisCache& cache,
                                             const TrackAnalysisOptions& options = {});
