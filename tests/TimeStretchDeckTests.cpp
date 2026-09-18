// SPDX-License-Identifier: AGPL-3.0-only
#include "core/TimeStretchDeck.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
constexpr double pi = 3.14159265358979323846;
int checks = 0;

void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

void fillTone(std::vector<float>& left, std::vector<float>& right, double& phase,
              double frequency, double sampleRate) {
    const double advance = 2.0 * pi * frequency / sampleRate;
    for (std::size_t i = 0; i < left.size(); ++i) {
        const float value = static_cast<float>(0.2 * std::sin(phase));
        left[i] = value;
        right[i] = value * 0.75f;
        phase += advance;
        if (phase >= 2.0 * pi) phase -= 2.0 * pi;
    }
}

double estimateFrequency(const std::vector<float>& samples, double sampleRate) {
    std::size_t crossings = 0;
    for (std::size_t i = 1; i < samples.size(); ++i) {
        if (samples[i - 1] <= 0.0f && samples[i] > 0.0f) ++crossings;
    }
    const double duration = samples.size() > 1
        ? static_cast<double>(samples.size() - 1) / sampleRate : 0.0;
    return duration > 0.0 ? static_cast<double>(crossings) / duration : 0.0;
}

void run() {
    broke::TimeStretchDeckAdapter deck;
    check(!deck.prepared(), "deck adapter starts unprepared");
    check(!deck.prepare(0.0, 1024), "invalid sample rate rejected");
    check(!deck.prepare(48000.0, 0), "invalid output bound rejected");
    check(!deck.prepare(48000.0, 20000, 4.0), "input bound overflow rejected");
    check(deck.prepare(48000.0, 4096, 4.0), "deck adapter prepares");
    check(deck.maxOutputFrames() == 4096, "prepared output bound exposed");
    check(deck.maxInputFrames() >= 16384, "prepared input bound covers maximum rate");
    check(deck.inputLatencyFrames() > 0 && deck.outputLatencyFrames() > 0,
          "latency metadata remains exposed through deck adapter");
    check(deck.seekLengthFrames() > 0 && deck.seekLengthFrames() <= deck.maxInputFrames(),
          "seek preroll fits prepared input bound");

    check(!deck.setPlaybackRate(0.0), "zero playback rate rejected");
    check(!deck.setPlaybackRate(4.1), "rate above prepared maximum rejected");
    check(deck.setPlaybackRate(1.25), "1.25x playback rate accepted");
    check(deck.inputFramesForOutput(1024) == 1280, "1.25x clock requests exact source block");

    const int inputFrames = deck.inputFramesForOutput(1024);
    std::vector<float> left(static_cast<std::size_t>(inputFrames));
    std::vector<float> right(static_cast<std::size_t>(inputFrames));
    std::vector<float> outLeft(1024);
    std::vector<float> outRight(1024);
    double phase = 0.0;
    fillTone(left, right, phase, 440.0, 48000.0);

    const auto beforeFailure = deck.sourceFramesConsumed();
    const auto carryBeforeFailure = deck.fractionalSourceCarry();
    check(!deck.processStereo(left.data(), right.data(), inputFrames - 1,
                              outLeft.data(), outRight.data(), 1024),
          "mismatched source block is rejected for fallback");
    check(deck.sourceFramesConsumed() == beforeFailure,
          "failed block does not advance source clock");
    check(deck.fractionalSourceCarry() == carryBeforeFailure,
          "failed block does not change fractional clock carry");

    std::vector<float> captured;
    captured.reserve(80U * outLeft.size());
    for (int block = 0; block < 120; ++block) {
        const int needed = deck.inputFramesForOutput(1024);
        left.resize(static_cast<std::size_t>(needed));
        right.resize(static_cast<std::size_t>(needed));
        fillTone(left, right, phase, 440.0, 48000.0);
        check(deck.processStereo(left.data(), right.data(), needed,
                                 outLeft.data(), outRight.data(), 1024),
              "bounded deck block accepted");
        if (block >= 40) captured.insert(captured.end(), outLeft.begin(), outLeft.end());
    }
    const double lockedHz = estimateFrequency(captured, 48000.0);
    check(std::abs(lockedHz - 440.0) < 5.0,
          "deck clock 1.25x path preserves source pitch");
    check(deck.sourceFramesConsumed() == 120LL * 1280LL,
          "1.25x deck clock accounts exact source consumption");

    deck.reset();
    check(deck.setPlaybackRate(1.001), "fractional playback rate accepted");
    constexpr int fractionalOutputFrames = 257;
    constexpr int fractionalBlocks = 1000;
    std::int64_t planned = 0;
    for (int block = 0; block < fractionalBlocks; ++block) {
        const int needed = deck.inputFramesForOutput(fractionalOutputFrames);
        check(needed > 0, "fractional-rate block has bounded positive source request");
        planned += needed;
        std::vector<float> inLeft(static_cast<std::size_t>(needed), 0.0f);
        std::vector<float> inRight(static_cast<std::size_t>(needed), 0.0f);
        std::vector<float> outL(fractionalOutputFrames, 0.0f);
        std::vector<float> outR(fractionalOutputFrames, 0.0f);
        check(deck.processStereo(inLeft.data(), inRight.data(), needed,
                                 outL.data(), outR.data(), fractionalOutputFrames),
              "fractional-rate clock block processes");
    }
    const auto expected = static_cast<std::int64_t>(std::floor(
        static_cast<double>(fractionalOutputFrames * fractionalBlocks) * 1.001));
    check(planned == expected, "fractional carry prevents cumulative source-clock drift");
    check(deck.sourceFramesConsumed() == expected,
          "reported source consumption matches fractional clock plan");
    check(deck.fractionalSourceCarry() >= 0.0 && deck.fractionalSourceCarry() < 1.0,
          "fractional carry stays normalized");

    const int seekFrames = deck.seekLengthFrames();
    std::vector<float> seekLeft(static_cast<std::size_t>(seekFrames));
    std::vector<float> seekRight(static_cast<std::size_t>(seekFrames));
    double seekPhase = 0.0;
    fillTone(seekLeft, seekRight, seekPhase, 330.0, 48000.0);
    check(deck.primeAfterDiscontinuity(seekLeft.data(), seekRight.data(), seekFrames),
          "seek/load discontinuity preroll accepted");
    check(deck.sourceFramesConsumed() == 0 && deck.fractionalSourceCarry() == 0.0,
          "discontinuity resets source clock state");

    check(deck.setPitchSemitones(12.0f), "deck adapter forwards bounded pitch control");
    check(!deck.setPitchSemitones(25.0f), "deck adapter rejects out-of-range pitch");
    check(deck.inputFramesForOutput(deck.maxOutputFrames() + 1) == 0,
          "oversized output request is rejected before processing");

    std::cout << "METRIC deck_keylock_1_25x_hz=" << lockedHz
              << " fractional_clock_source_frames=" << expected
              << " carry=" << deck.fractionalSourceCarry()
              << " input_latency_frames=" << deck.inputLatencyFrames()
              << " output_latency_frames=" << deck.outputLatencyFrames() << '\n';
}
} // namespace

int main() {
    try {
        run();
        std::cout << "PASS: " << checks << " time-stretch deck-clock checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
