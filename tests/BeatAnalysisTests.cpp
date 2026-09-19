// SPDX-License-Identifier: AGPL-3.0-only
#include "core/BeatAnalysis.h"
#include "core/BeatGridPerformance.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
int checks = 0;
void check(bool condition, const char* name) {
    ++checks;
    if (!condition) throw std::runtime_error(name);
}

struct Signal final {
    std::vector<float> left;
    std::vector<float> right;
};

Signal clickTrack(double sampleRate, double seconds, double bpm, double firstBeatSeconds,
                  bool alternating = false) {
    const auto frames = static_cast<std::size_t>(std::llround(sampleRate * seconds));
    Signal signal{std::vector<float>(frames, 0.0f), std::vector<float>(frames, 0.0f)};
    const double interval = 60.0 / bpm;
    int beat = 0;
    for (double t = firstBeatSeconds; t < seconds; t += interval, ++beat) {
        const auto start = static_cast<std::size_t>(std::llround(t * sampleRate));
        const float strength = alternating && (beat % 2 != 0) ? 0.35f : 0.95f;
        for (std::size_t i = 0; i < 96 && start + i < frames; ++i) {
            const float decay = std::exp(-static_cast<float>(i) / 18.0f);
            signal.left[start + i] += strength * decay;
            signal.right[start + i] += 0.82f * strength * decay;
        }
    }
    return signal;
}

void checkEstimate(double bpm, double firstBeat, bool alternating) {
    constexpr double sampleRate = 48000.0;
    auto signal = clickTrack(sampleRate, 32.0, bpm, firstBeat, alternating);
    const auto result = broke::analyzeBeatGrid(signal.left.data(), signal.right.data(),
                                               signal.left.size(), sampleRate);
    check(result.valid, "synthetic click track produces a beat estimate");
    check(std::abs(result.bpm - bpm) < 0.75, "BPM estimate stays within fixture tolerance");
    const double interval = 60.0 / bpm;
    const double beatOffset = (result.beatZeroSeconds - firstBeat) / interval;
    check(std::abs(beatOffset - std::round(beatOffset)) < 0.06,
          "beat-grid anchor preserves the detected beat phase");
    check(result.beatZeroSeconds >= std::max(0.0, firstBeat - 0.05)
          && result.beatZeroSeconds <= firstBeat + 4.0 * interval,
          "beat-grid anchor stays near the first audible beat region");
    check(result.confidence >= 0.18 && result.confidence <= 1.0,
          "confidence is finite and bounded");
    check(result.segments.size() == 1, "constant-tempo analysis yields one grid segment");
    check(std::abs(result.segments.front().bpm - result.bpm) < 1.0e-9,
          "grid segment uses the reported tempo");
}

void chunkingIsDeterministic() {
    constexpr double sampleRate = 44100.0;
    auto signal = clickTrack(sampleRate, 25.0, 128.0, 0.37, true);
    auto direct = broke::analyzeBeatGrid(signal.left.data(), signal.right.data(),
                                         signal.left.size(), sampleRate);
    broke::BeatAnalysisAccumulator streamed(sampleRate);
    std::size_t offset = 0;
    const std::size_t chunks[] {17, 509, 4096, 113, 7777, 53};
    std::size_t which = 0;
    while (offset < signal.left.size()) {
        const auto count = std::min(chunks[which++ % std::size(chunks)], signal.left.size() - offset);
        streamed.push(signal.left.data() + offset, signal.right.data() + offset, count);
        offset += count;
    }
    auto chunked = streamed.finish();
    check(direct.valid && chunked.valid, "direct and chunked analysis are valid");
    check(std::abs(direct.bpm - chunked.bpm) < 1.0e-9, "chunking does not change BPM");
    check(std::abs(direct.beatZeroSeconds - chunked.beatZeroSeconds) < 1.0e-9,
          "chunking does not change beat-grid anchor");
}

void editableBeatGridSupportsTempoChanges() {
    broke::BeatAnalysisResult analysis;
    analysis.valid = true;
    analysis.bpm = 120.0;
    analysis.beatZeroSeconds = 0.5;
    analysis.confidence = 0.9;
    analysis.segments.push_back({0.5, 120.0});

    broke::BeatGrid grid(analysis);
    check(grid.valid(), "analysis result seeds a valid editable grid");
    check(grid.segments().size() == 1, "seeded grid starts with one tempo segment");
    check(std::abs(grid.beatAtTime(1.0) - 1.0) < 1.0e-9,
          "single-tempo grid maps time to beats");
    check(std::abs(grid.timeAtBeat(4.0) - 2.5) < 1.0e-9,
          "single-tempo grid maps beats to time");
    check(std::abs(grid.quantizeTime(1.24, 0.5) - 1.25) < 1.0e-9,
          "grid quantization uses musical beat steps");

    check(grid.insertTempoChangeAtBeat(8.0, 90.0),
          "tempo change can be inserted at an exact beat");
    check(grid.segments().size() == 2, "tempo map contains the inserted segment");
    check(std::abs(grid.segments()[1].startSeconds - 4.5) < 1.0e-9,
          "tempo change boundary preserves beat continuity");
    check(std::abs(grid.beatAtTime(4.5) - 8.0) < 1.0e-9,
          "tempo boundary maps to the requested beat");
    check(std::abs(grid.timeAtBeat(10.0) - (4.5 + 120.0 / 90.0)) < 1.0e-9,
          "post-change beat mapping uses the new tempo");
    check(std::abs(grid.beatAtTime(grid.timeAtBeat(13.75)) - 13.75) < 1.0e-9,
          "variable-tempo beat/time mapping is reversible");

    check(grid.setSegmentBpm(1, 100.0), "tempo segment can be edited safely");
    check(std::abs(grid.timeAtBeat(10.0) - 5.7) < 1.0e-9,
          "edited segment tempo immediately changes mapping");
    check(!grid.setSegmentBpm(1, std::numeric_limits<double>::quiet_NaN()),
          "non-finite manual tempo is rejected without corrupting the grid");
    check(!grid.insertTempoChangeAtBeat(8.0, 110.0),
          "duplicate tempo-change boundary is rejected");

    check(grid.setBeatZero(0.75), "beat-zero edit shifts the complete tempo map");
    check(std::abs(grid.segments()[0].startSeconds - 0.75) < 1.0e-9
          && std::abs(grid.segments()[1].startSeconds - 4.75) < 1.0e-9,
          "beat-zero edit preserves relative tempo-change boundaries");
    check(std::abs(grid.beatAtTime(0.25) + 1.0) < 1.0e-9,
          "times before beat zero retain a meaningful negative beat position");

    check(grid.removeTempoChange(1), "later tempo change can be removed");
    check(grid.segments().size() == 1 && grid.valid(),
          "tempo-map removal returns to a valid single-tempo grid");
    check(!grid.removeTempoChange(0), "base grid segment cannot be removed");

    broke::BeatGrid transactional;
    check(transactional.reset(0.25, 128.0), "manual grid can be initialized directly");
    const auto oldZero = transactional.beatZeroSeconds();
    check(!transactional.setBeatZero(-1.0), "negative beat zero is rejected transactionally");
    check(std::abs(transactional.beatZeroSeconds() - oldZero) < 1.0e-12,
          "rejected edit leaves the previous grid untouched");
    check(std::isnan(transactional.quantizeTime(1.0, 0.0)),
          "invalid quantization step fails closed");

    broke::BeatAnalysisResult invalidAnalysis;
    invalidAnalysis.valid = true;
    invalidAnalysis.bpm = 120.0;
    invalidAnalysis.beatZeroSeconds = 0.5;
    invalidAnalysis.segments = {{0.5, 120.0}, {0.5, 130.0}};
    check(!transactional.reset(invalidAnalysis),
          "non-increasing imported tempo map is rejected");
    check(transactional.valid(), "failed import does not destroy the existing grid");
}

void performancePlansRespectReviewedGrid() {
    broke::BeatGrid grid;
    check(grid.reset(0.5, 120.0), "performance fixture grid initializes");
    check(grid.insertTempoChangeAtBeat(8.0, 90.0),
          "performance fixture contains a variable-tempo segment");

    check(std::abs(broke::beatGridBpmAtTime(grid, 4.49) - 120.0) < 1.0e-9,
          "local BPM reports the segment before a tempo boundary");
    check(std::abs(broke::beatGridBpmAtTime(grid, 4.5) - 90.0) < 1.0e-9,
          "local BPM changes exactly at a reviewed boundary");

    const auto previous = broke::quantizedBeatTime(
        grid, 1.30, 1.0, broke::QuantizeDirection::previous);
    const auto nearest = broke::quantizedBeatTime(
        grid, 1.30, 1.0, broke::QuantizeDirection::nearest);
    const auto next = broke::quantizedBeatTime(
        grid, 1.30, 1.0, broke::QuantizeDirection::next);
    check(std::abs(previous - 1.0) < 1.0e-9,
          "previous-beat quantization never advances the hotcue target");
    check(std::abs(nearest - 1.5) < 1.0e-9,
          "nearest-beat quantization uses the reviewed grid phase");
    check(std::abs(next - 1.5) < 1.0e-9,
          "next-beat quantization advances to the next reviewed beat");
    check(std::abs(broke::quantizedBeatTime(
        grid, 1.5, 1.0, broke::QuantizeDirection::next) - 1.5) < 1.0e-9,
          "next quantization keeps an already exact beat stable");

    const auto loop = broke::planBeatLoop(grid, 3.7, 4.0);
    check(loop.valid, "four-beat loop plan is valid on a reviewed grid");
    check(std::abs(loop.startBeat - 6.0) < 1.0e-9 && std::abs(loop.startSeconds - 3.5) < 1.0e-9,
          "beat loop snaps its start to the preceding reviewed beat");
    check(std::abs(loop.endSeconds - (4.5 + 120.0 / 90.0)) < 1.0e-9,
          "beat loop crossing a tempo change resolves its endpoint in beat space");
    check(std::abs(grid.beatAtTime(loop.endSeconds) - 10.0) < 1.0e-9,
          "variable-tempo loop preserves the requested musical length");

    broke::BeatGrid master;
    broke::BeatGrid follower;
    check(master.reset(0.25, 128.0), "master sync grid initializes");
    check(follower.reset(0.5, 120.0), "follower sync grid initializes");
    const auto sync = broke::planBeatSync(follower, 2.05, master, 2.125);
    check(sync.valid, "compatible reviewed grids produce a sync plan");
    check(std::abs(sync.followerRate - (128.0 / 120.0)) < 1.0e-12,
          "sync plan derives rate from local grid tempos");
    check(std::abs(sync.masterBeat - 4.0) < 1.0e-9,
          "sync plan exposes the master musical phase");
    check(std::abs(sync.followerTargetBeat - 3.0) < 1.0e-9
          && std::abs(sync.followerTargetSeconds - 2.0) < 1.0e-9,
          "sync plan resolves a deterministic follower phase target");
    check(std::abs(sync.phaseErrorBeats + 0.1) < 1.0e-9,
          "sync plan reports signed phase correction instead of hiding a seek");

    broke::BeatGrid slow;
    broke::BeatGrid fast;
    check(slow.reset(0.0, 70.0) && fast.reset(0.0, 180.0),
          "mismatched sync fixture grids initialize");
    check(!broke::planBeatSync(slow, 2.0, fast, 2.0).valid,
          "unsafe tempo ratio fails closed instead of exceeding deck rate bounds");
    check(!broke::planBeatLoop(grid, 1.0, 0.0).valid,
          "zero-length musical loop fails closed");
    check(std::isnan(broke::quantizedBeatTime(grid, 1.0, 0.0)),
          "invalid hotcue quantization step fails closed");
}

void nonPeriodicMaterialDoesNotFabricateTempo() {
    constexpr double sampleRate = 48000.0;
    constexpr double seconds = 12.0;
    const auto frames = static_cast<std::size_t>(sampleRate * seconds);

    std::vector<float> steady(frames, 0.0f);
    for (std::size_t i = 0; i < frames; ++i) {
        const double phase = 2.0 * 3.14159265358979323846 * 440.0
            * static_cast<double>(i) / sampleRate;
        steady[i] = 0.25f * static_cast<float>(std::sin(phase));
    }
    const auto tone = broke::analyzeBeatGrid(steady.data(), nullptr, steady.size(), sampleRate);
    check(!tone.valid, "steady tone does not fabricate tempo");

    std::vector<float> noise(frames, 0.0f);
    std::uint32_t state = 0x6d2b79f5U;
    for (auto& sample : noise) {
        state = state * 1664525U + 1013904223U;
        const double unit = static_cast<double>(state) / 4294967295.0;
        sample = static_cast<float>((unit * 2.0 - 1.0) * 0.2);
    }
    const auto broadband = broke::analyzeBeatGrid(noise.data(), nullptr, noise.size(), sampleRate);
    check(!broadband.valid, "deterministic broadband noise does not fabricate tempo");
}

void invalidAndNonFiniteInputsFailSafely() {
    std::vector<float> silence(48000 * 6, 0.0f);
    auto silent = broke::analyzeBeatGrid(silence.data(), nullptr, silence.size(), 48000.0);
    check(!silent.valid, "silence does not fabricate tempo");

    auto noisy = clickTrack(48000.0, 10.0, 120.0, 0.2);
    noisy.left[100] = std::numeric_limits<float>::quiet_NaN();
    noisy.right[101] = std::numeric_limits<float>::infinity();
    auto finite = broke::analyzeBeatGrid(noisy.left.data(), noisy.right.data(), noisy.left.size(), 48000.0);
    check(finite.valid && std::isfinite(finite.bpm), "NaN/Inf samples are sanitized off-thread");

    broke::BeatAnalysisOptions bad;
    bad.minBpm = 180.0;
    bad.maxBpm = 70.0;
    broke::BeatAnalysisAccumulator invalid(48000.0, bad);
    check(!invalid.configured(), "invalid tempo range fails closed");
    invalid.push(noisy.left.data(), noisy.right.data(), noisy.left.size());
    check(!invalid.finish().valid, "invalid analyzer cannot emit a result");
}

void analysisWorkIsBounded() {
    constexpr double sampleRate = 48000.0;
    auto signal = clickTrack(sampleRate, 12.0, 120.0, 0.2);
    broke::BeatAnalysisOptions options;
    options.maxAnalysisSeconds = 5.0;
    broke::BeatAnalysisAccumulator bounded(sampleRate, options);
    bounded.push(signal.left.data(), signal.right.data(), signal.left.size());
    const auto result = bounded.finish();
    check(result.truncated, "analysis reports max-duration truncation");
    check(result.analyzedSeconds <= 5.01, "analysis duration stays within configured cap");
    check(result.valid, "bounded prefix can still produce a valid tempo estimate");
}
}

int main() {
    try {
        checkEstimate(120.0, 0.25, false);
        checkEstimate(128.0, 0.37, true);
        checkEstimate(90.0, 1.10, false);
        chunkingIsDeterministic();
        editableBeatGridSupportsTempoChanges();
        performancePlansRespectReviewedGrid();
        nonPeriodicMaterialDoesNotFabricateTempo();
        invalidAndNonFiniteInputsFailSafely();
        analysisWorkIsBounded();
        std::cout << "PASS: " << checks << " beat-analysis/grid checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
