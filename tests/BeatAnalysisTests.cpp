// SPDX-License-Identifier: AGPL-3.0-only
#include "core/BeatAnalysis.h"

#include <algorithm>
#include <cmath>
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
        invalidAndNonFiniteInputsFailSafely();
        analysisWorkIsBounded();
        std::cout << "PASS: " << checks << " beat-analysis checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
