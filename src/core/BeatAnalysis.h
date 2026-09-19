// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace broke {

struct BeatGridSegment final {
    double startSeconds = 0.0;
    double bpm = 0.0;
};

struct BeatAnalysisResult final {
    bool valid = false;
    double bpm = 0.0;
    double beatZeroSeconds = 0.0;
    double confidence = 0.0;
    double analyzedSeconds = 0.0;
    bool truncated = false;
    std::vector<BeatGridSegment> segments;
};

struct BeatAnalysisOptions final {
    double minBpm = 70.0;
    double maxBpm = 180.0;
    double envelopeRate = 200.0;
    double maxAnalysisSeconds = 1800.0;
};

// Streaming/offline beat analysis. Audio is reduced to a compact energy envelope
// while samples are pushed, so callers can analyze long files without retaining
// the full decoded track in memory. This class is not realtime-safe and is
// intended for background workers only.
class BeatAnalysisAccumulator final {
public:
    BeatAnalysisAccumulator(double sampleRate, BeatAnalysisOptions options = {});

    [[nodiscard]] bool configured() const noexcept { return configuredFlag; }
    void push(const float* left, const float* right, std::size_t frames);
    [[nodiscard]] BeatAnalysisResult finish();

private:
    void emitEnvelopePoint();

    BeatAnalysisOptions options;
    double sourceRate = 0.0;
    double actualEnvelopeRate = 0.0;
    std::size_t samplesPerEnvelope = 0;
    std::size_t maxEnvelopePoints = 0;
    std::size_t samplesInCurrent = 0;
    double currentAbsSum = 0.0;
    std::uint64_t acceptedSamples = 0;
    bool configuredFlag = false;
    bool truncatedFlag = false;
    bool finishedFlag = false;
    std::vector<float> envelope;
};

[[nodiscard]] BeatAnalysisResult analyzeBeatGrid(const float* left,
                                                 const float* right,
                                                 std::size_t frames,
                                                 double sampleRate,
                                                 BeatAnalysisOptions options = {});

} // namespace broke
