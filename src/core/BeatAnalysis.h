// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
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

    // UI/controller-facing segment edits remain transactional. Segment zero is
    // the immutable beat-zero boundary and cannot be moved or removed. Moving a
    // later boundary first removes that boundary from a candidate copy and then
    // resolves the requested musical beat against the remaining map, so a failed
    // edit never leaves the live reviewed grid half-mutated.
    [[nodiscard]] bool moveTempoChangeToBeat(std::size_t index, double beat) {
        if (!validate() || index == 0 || index >= tempoMap.size()) return false;
        BeatGrid candidate = *this;
        const double bpm = candidate.tempoMap[index].bpm;
        candidate.tempoMap.erase(candidate.tempoMap.begin() + static_cast<std::ptrdiff_t>(index));
        if (!candidate.insertTempoChangeAtBeat(beat, bpm)) return false;
        *this = candidate;
        return true;
    }

    [[nodiscard]] bool replaceTempoChange(std::size_t index, double beat, double bpm) {
        if (!validate() || index == 0 || index >= tempoMap.size()) return false;
        BeatGrid candidate = *this;
        if (!candidate.moveTempoChangeToBeat(index, beat)) return false;
        // Moving can change ordering when a boundary crosses another segment, so
        // identify the moved boundary by its requested beat rather than reusing
        // the old numeric index.
        const double targetSeconds = candidate.timeAtBeat(beat);
        if (!std::isfinite(targetSeconds)) return false;
        std::size_t movedIndex = candidate.tempoMap.size();
        for (std::size_t i = 1; i < candidate.tempoMap.size(); ++i) {
            if (std::abs(candidate.tempoMap[i].startSeconds - targetSeconds) <= 1.0e-8) {
                movedIndex = i;
                break;
            }
        }
        if (movedIndex >= candidate.tempoMap.size() || !candidate.setSegmentBpm(movedIndex, bpm))
            return false;
        *this = candidate;
        return true;
    }

    [[nodiscard]] double tempoChangeBeat(std::size_t index) const noexcept {
        if (!validate() || index >= tempoMap.size())
            return std::numeric_limits<double>::quiet_NaN();
        return beatAtTime(tempoMap[index].startSeconds);
    }

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
