// SPDX-License-Identifier: AGPL-3.0-only
#include "core/TimeStretch.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
constexpr double pi = 3.14159265358979323846;
int checks = 0;

void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

struct Oscillator final {
    double phase = 0.0;
    double frequency = 440.0;
    double sampleRate = 48000.0;

    void fill(float* left, float* right, int frames) {
        const double advance = 2.0 * pi * frequency / sampleRate;
        for (int i = 0; i < frames; ++i) {
            const float value = static_cast<float>(0.25 * std::sin(phase));
            left[i] = value;
            right[i] = value;
            phase += advance;
            if (phase >= 2.0 * pi) phase -= 2.0 * pi;
        }
    }
};

double estimateFrequency(const std::vector<float>& samples, double sampleRate) {
    if (samples.size() < 2) return 0.0;
    std::size_t crossings = 0;
    for (std::size_t i = 1; i < samples.size(); ++i) {
        if (samples[i - 1] <= 0.0f && samples[i] > 0.0f) ++crossings;
    }
    const double duration = static_cast<double>(samples.size() - 1) / sampleRate;
    return duration > 0.0 ? static_cast<double>(crossings) / duration : 0.0;
}

std::vector<float> renderTone(broke::TimeStretchPrototype& processor, double sourceFrequency,
                              int inputFrames, int outputFrames, int blocks, int discardBlocks) {
    std::vector<float> captured;
    captured.reserve(static_cast<std::size_t>(std::max(0, blocks - discardBlocks) * outputFrames));
    std::vector<float> left(static_cast<std::size_t>(inputFrames));
    std::vector<float> right(static_cast<std::size_t>(inputFrames));
    std::vector<float> outLeft(static_cast<std::size_t>(outputFrames));
    std::vector<float> outRight(static_cast<std::size_t>(outputFrames));
    Oscillator oscillator{0.0, sourceFrequency, 48000.0};

    for (int block = 0; block < blocks; ++block) {
        oscillator.fill(left.data(), right.data(), inputFrames);
        check(processor.processStereo(left.data(), right.data(), inputFrames,
                                      outLeft.data(), outRight.data(), outputFrames),
              "bounded time-stretch block accepted");
        check(std::all_of(outLeft.begin(), outLeft.end(), [](float value) { return std::isfinite(value); }),
              "time-stretch output remains finite");
        if (block >= discardBlocks) captured.insert(captured.end(), outLeft.begin(), outLeft.end());
    }
    return captured;
}

void run() {
    broke::TimeStretchPrototype processor;
    check(!processor.prepared(), "prototype starts unprepared");
    check(!processor.prepare(0.0, 8192, 4096), "invalid sample rate rejected");
    check(!processor.prepare(48000.0, 0, 4096), "invalid input bound rejected");
    check(processor.prepare(48000.0, 8192, 4096), "prototype prepares at 48 kHz");
    check(processor.prepared(), "prepared state reported");
    check(processor.inputLatencyFrames() > 0, "input latency exposed");
    check(processor.outputLatencyFrames() > 0, "output latency exposed");
    check(processor.seekLengthFrames() > 0 && processor.seekLengthFrames() <= processor.maxInputFrames(),
          "prepared input bound can hold recommended seek preroll");
    check(processor.maxInputFrames() == 8192 && processor.maxOutputFrames() == 4096,
          "configured block bounds exposed");
    check(!processor.setPitchSemitones(std::numeric_limits<float>::quiet_NaN()), "NaN pitch rejected");
    check(!processor.setPitchSemitones(25.0f), "out-of-range pitch rejected");
    check(processor.setPitchSemitones(0.0f), "neutral pitch accepted");

    std::array<float, 32> tiny{};
    check(!processor.processStereo(nullptr, tiny.data(), 32, tiny.data(), tiny.data(), 32),
          "null input rejected");
    check(!processor.processStereo(tiny.data(), tiny.data(), 8193, tiny.data(), tiny.data(), 32),
          "oversized input block rejected before touching input");
    check(!processor.processStereo(tiny.data(), tiny.data(), 32, tiny.data(), tiny.data(), 4097),
          "oversized output block rejected");

    // Consume 1.25 input frames per output frame. A pitch-changing resampler
    // would move 440 Hz toward 550 Hz; key-locked stretch should remain near 440 Hz.
    processor.reset();
    auto locked = renderTone(processor, 440.0, 1280, 1024, 120, 40);
    const double lockedHz = estimateFrequency(locked, 48000.0);
    check(std::abs(lockedHz - 440.0) < 5.0, "1.25x time ratio preserves source pitch");

    processor.reset();
    check(processor.setPitchSemitones(12.0f), "one-octave pitch shift accepted");
    auto shifted = renderTone(processor, 440.0, 1024, 1024, 120, 40);
    const double shiftedHz = estimateFrequency(shifted, 48000.0);
    check(std::abs(shiftedHz - 880.0) < 8.0, "pitch control raises a 440 Hz fixture by one octave");

    processor.reset();
    check(processor.setPitchSemitones(0.0f), "pitch can return to neutral");
    const int seekFrames = processor.seekLengthFrames();
    std::vector<float> seekLeft(static_cast<std::size_t>(seekFrames));
    std::vector<float> seekRight(static_cast<std::size_t>(seekFrames));
    Oscillator seekOscillator{0.0, 330.0, 48000.0};
    seekOscillator.fill(seekLeft.data(), seekRight.data(), seekFrames);
    check(processor.primeAfterSeek(seekLeft.data(), seekRight.data(), seekFrames, 1.0),
          "seek preroll accepted");
    check(!processor.primeAfterSeek(seekLeft.data(), seekRight.data(), seekFrames, 0.0),
          "invalid seek playback rate rejected");
    auto postSeek = renderTone(processor, 330.0, 1024, 1024, 40, 20);
    check(std::any_of(postSeek.begin(), postSeek.end(), [](float value) { return std::abs(value) > 1.0e-5f; }),
          "post-seek processing produces non-silent finite output");

    std::cout << "METRIC keylock_1_25x_hz=" << lockedHz
              << " octave_shift_hz=" << shiftedHz
              << " input_latency_frames=" << processor.inputLatencyFrames()
              << " output_latency_frames=" << processor.outputLatencyFrames()
              << " seek_length_frames=" << processor.seekLengthFrames() << '\n';
}
} // namespace

int main() {
    try {
        run();
        std::cout << "PASS: " << checks << " time-stretch prototype checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
