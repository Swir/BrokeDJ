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

// Editable tempo map used by deck/sync work. This is intentionally independent
// from JUCE and the audio callback: editing, validation and beat/time conversion
// happen on owner/UI workers and can later be persisted by the library layer.
// Segment 0 always starts at beat zero. Later segments preserve continuous beat
// numbering while allowing genuine tempo changes instead of forcing one BPM.
class BeatGrid final {
public:
    static constexpr std::size_t maxSegments = 128;

    BeatGrid() = default;
    explicit BeatGrid(const BeatAnalysisResult& analysis);

    [[nodiscard]] bool reset(double beatZeroSeconds, double bpm);
    [[nodiscard]] bool reset(const BeatAnalysisResult& analysis);
    [[nodiscard]] bool valid() const noexcept;

    // Moving beat zero shifts every tempo-change boundary by the same amount,
    // preserving the edited musical structure. Negative absolute time is rejected.
    [[nodiscard]] bool setBeatZero(double seconds);
    [[nodiscard]] bool setSegmentBpm(std::size_t index, double bpm);
    [[nodiscard]] bool insertTempoChangeAtBeat(double beat, double bpm);
    [[nodiscard]] bool removeTempoChange(std::size_t index);

    [[nodiscard]] double beatZeroSeconds() const noexcept { return beatZero; }
    [[nodiscard]] const std::vector<BeatGridSegment>& segments() const noexcept { return tempoMap; }

    // Beat zero is beat 0. Negative beat numbers/times before beat zero are valid
    // and use the first segment tempo. Non-finite/invalid requests return NaN.
    [[nodiscard]] double beatAtTime(double seconds) const noexcept;
    [[nodiscard]] double timeAtBeat(double beat) const noexcept;
    [[nodiscard]] double quantizeTime(double seconds, double beatStep = 1.0) const noexcept;

private:
    [[nodiscard]] static bool validBpm(double bpm) noexcept;
    [[nodiscard]] bool validate() const noexcept;

    double beatZero = 0.0;
    std::vector<BeatGridSegment> tempoMap;
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
