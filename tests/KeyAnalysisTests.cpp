// SPDX-License-Identifier: AGPL-3.0-only
#include "core/KeyAnalysis.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
int checks = 0;
void check(bool condition, const char* name) {
    ++checks;
    if (!condition) throw std::runtime_error(name);
}

constexpr double pi = 3.141592653589793238462643383279502884;

double midiFrequency(int midi) {
    return 440.0 * std::pow(2.0, (static_cast<double>(midi) - 69.0) / 12.0);
}

std::vector<float> progression(double sampleRate, bool minor) {
    constexpr int majorChords[4][3] {{60, 64, 67}, {65, 69, 72}, {67, 71, 74}, {60, 64, 67}};
    constexpr int minorChords[4][3] {{57, 60, 64}, {62, 65, 69}, {64, 67, 71}, {57, 60, 64}};
    const int framesPerChord = static_cast<int>(std::llround(sampleRate * 2.0));
    std::vector<float> samples(static_cast<std::size_t>(framesPerChord * 4), 0.0f);
    for (int chord = 0; chord < 4; ++chord) {
        for (int frame = 0; frame < framesPerChord; ++frame) {
            const double time = static_cast<double>(frame) / sampleRate;
            double value = 0.0;
            for (int note = 0; note < 3; ++note) {
                const int midi = minor ? minorChords[chord][note] : majorChords[chord][note];
                value += std::sin(2.0 * pi * midiFrequency(midi) * time);
            }
            samples[static_cast<std::size_t>(chord * framesPerChord + frame)]
                = static_cast<float>(0.25 * value / 3.0);
        }
    }
    return samples;
}

void progressionFixturesIdentifyMode() {
    constexpr double sampleRate = 48000.0;
    const auto major = progression(sampleRate, false);
    const auto majorResult = broke::analyzeMusicalKey(
        major.data(), nullptr, major.size(), sampleRate);
    check(majorResult.valid, "major progression publishes a key");
    check(majorResult.tonic == 0, "C-major fixture reports C tonic");
    check(majorResult.mode == broke::KeyMode::major, "C-major fixture reports major mode");
    check(majorResult.confidence > 0.25 && majorResult.confidence <= 1.0,
          "major key confidence is finite and bounded");

    const auto minor = progression(sampleRate, true);
    const auto minorResult = broke::analyzeMusicalKey(
        minor.data(), nullptr, minor.size(), sampleRate);
    check(minorResult.valid, "minor progression publishes a key");
    check(minorResult.tonic == 9, "A-minor fixture reports A tonic");
    check(minorResult.mode == broke::KeyMode::minor, "A-minor fixture reports minor mode");
    check(minorResult.confidence > 0.25 && minorResult.confidence <= 1.0,
          "minor key confidence is finite and bounded");
}

void chunkingDoesNotChangeKey() {
    constexpr double sampleRate = 44100.0;
    const auto samples = progression(sampleRate, false);
    broke::KeyAnalysisAccumulator accumulator(sampleRate);
    std::size_t offset = 0;
    const std::size_t chunks[] {17, 509, 4096, 73, 8191};
    std::size_t which = 0;
    while (offset < samples.size()) {
        const auto count = std::min(chunks[which++ % std::size(chunks)], samples.size() - offset);
        accumulator.push(samples.data() + offset, nullptr, count);
        offset += count;
    }
    const auto result = accumulator.finish();
    check(result.valid && result.tonic == 0 && result.mode == broke::KeyMode::major,
          "chunked analysis preserves the C-major result");
}

void nonTonalMaterialFailsClosed() {
    constexpr double sampleRate = 48000.0;
    const auto frames = static_cast<std::size_t>(sampleRate * 6.0);
    std::vector<float> silence(frames, 0.0f);
    check(!broke::analyzeMusicalKey(silence.data(), nullptr, silence.size(), sampleRate).valid,
          "silence does not fabricate a musical key");

    std::vector<float> tone(frames, 0.0f);
    for (std::size_t i = 0; i < tone.size(); ++i) {
        tone[i] = 0.2f * static_cast<float>(std::sin(
            2.0 * pi * 261.625565 * static_cast<double>(i) / sampleRate));
    }
    check(!broke::analyzeMusicalKey(tone.data(), nullptr, tone.size(), sampleRate).valid,
          "a single steady tone is not promoted to a song key");

    std::uint32_t state = 0x12345678U;
    std::vector<float> noise(frames, 0.0f);
    for (auto& sample : noise) {
        state = state * 1664525U + 1013904223U;
        const double unit = static_cast<double>(state) / 4294967295.0;
        sample = static_cast<float>((unit * 2.0 - 1.0) * 0.2);
    }
    check(!broke::analyzeMusicalKey(noise.data(), nullptr, noise.size(), sampleRate).valid,
          "deterministic broadband noise does not fabricate a musical key");
}

void invalidSamplesAndBoundsAreSafe() {
    constexpr double sampleRate = 48000.0;
    auto samples = progression(sampleRate, false);
    samples[100] = std::numeric_limits<float>::quiet_NaN();
    samples[200] = std::numeric_limits<float>::infinity();
    const auto sanitized = broke::analyzeMusicalKey(
        samples.data(), nullptr, samples.size(), sampleRate);
    check(sanitized.valid && sanitized.tonic == 0,
          "non-finite source samples are sanitized off-thread");

    broke::KeyAnalysisOptions invalid;
    invalid.targetRate = 1000.0;
    broke::KeyAnalysisAccumulator rejected(sampleRate, invalid);
    check(!rejected.configured(), "invalid key-analysis options fail closed");

    broke::KeyAnalysisOptions bounded;
    bounded.maxAnalysisSeconds = 4.0;
    broke::KeyAnalysisAccumulator capped(sampleRate, bounded);
    capped.push(samples.data(), nullptr, samples.size());
    const auto result = capped.finish();
    check(result.truncated, "key analysis reports max-duration truncation");
    check(result.analyzedSeconds <= 4.01, "key analysis stays within configured duration cap");
}

void keyNamesAreStable() {
    check(std::string(broke::keyName(0)) == "C", "C key name is stable");
    check(std::string(broke::keyName(9)) == "A", "A key name is stable");
    check(std::string(broke::keyName(-1)) == "?", "invalid key name fails closed");
}
}

int main() {
    try {
        progressionFixturesIdentifyMode();
        chunkingDoesNotChangeKey();
        nonTonalMaterialFailsClosed();
        invalidSamplesAndBoundsAreSafe();
        keyNamesAreStable();
        std::cout << "PASS: " << checks << " musical-key analysis checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
