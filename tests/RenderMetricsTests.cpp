// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace {
constexpr double outputRate = 48000.0;
constexpr int blockSize = 256;
int checks = 0;

void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

std::unique_ptr<broke::Clip> sineClip(double frequency, float amplitude = 0.25f, int seconds = 4) {
    auto clip = std::make_unique<broke::Clip>();
    clip->sampleRate = outputRate;
    const auto frames = static_cast<std::size_t>(outputRate * static_cast<double>(seconds));
    clip->left.resize(frames);
    clip->right.resize(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const auto phase = 2.0 * std::numbers::pi * frequency * static_cast<double>(i) / outputRate;
        const auto sample = amplitude * static_cast<float>(std::sin(phase));
        clip->left[i] = sample;
        clip->right[i] = sample;
    }
    return clip;
}

std::unique_ptr<broke::Clip> stepClip(int frames = 48000) {
    auto clip = std::make_unique<broke::Clip>();
    clip->sampleRate = outputRate;
    clip->left.assign(static_cast<std::size_t>(frames), 1.0f);
    const auto half = clip->left.size() / 2;
    std::fill(clip->left.begin() + static_cast<std::ptrdiff_t>(half), clip->left.end(), -1.0f);
    clip->right = clip->left;
    return clip;
}

std::vector<float> render(broke::Engine& engine, int samples) {
    std::vector<float> result;
    result.reserve(static_cast<std::size_t>(samples));
    std::array<std::array<float, blockSize>, 4> audio{};
    std::array<float*, 4> out{};
    for (std::size_t c = 0; c < out.size(); ++c) out[c] = audio[c].data();
    while (samples > 0) {
        const int count = std::min(samples, blockSize);
        engine.process(out.data(), static_cast<int>(out.size()), count);
        result.insert(result.end(), audio[0].begin(), audio[0].begin() + count);
        samples -= count;
    }
    return result;
}

void warmup(broke::Engine& engine, int samples = 8192) {
    static_cast<void>(render(engine, samples));
}

double rmsOf(const std::vector<float>& samples) {
    if (samples.empty()) return 0.0;
    double sumSquares = 0.0;
    for (const float sample : samples) {
        check(std::isfinite(sample), "render remains finite");
        const double value = sample;
        sumSquares += value * value;
    }
    return std::sqrt(sumSquares / static_cast<double>(samples.size()));
}

struct ToneMetrics final {
    double measuredFrequency = 0.0;
    double frequencyError = 0.0;
    double residualRatio = 0.0;
    double dc = 0.0;
    double rms = 0.0;
    float peak = 0.0f;
};

ToneMetrics analyseTone(const std::vector<float>& samples, double expectedFrequency) {
    check(samples.size() > 1000, "tone render has enough samples");
    double mean = 0.0;
    double sumSquares = 0.0;
    float peak = 0.0f;
    int crossings = 0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const double x = samples[i];
        check(std::isfinite(x), "tone render remains finite");
        mean += x;
        sumSquares += x * x;
        peak = std::max(peak, std::abs(samples[i]));
        if (i > 0 && samples[i - 1] <= 0.0f && samples[i] > 0.0f) ++crossings;
    }
    mean /= static_cast<double>(samples.size());
    const auto duration = static_cast<double>(samples.size() - 1) / outputRate;
    const double measuredFrequency = static_cast<double>(crossings) / duration;

    double sinProjection = 0.0;
    double cosProjection = 0.0;
    const double omega = 2.0 * std::numbers::pi * expectedFrequency / outputRate;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const double phase = omega * static_cast<double>(i);
        const double centered = static_cast<double>(samples[i]) - mean;
        sinProjection += centered * std::sin(phase);
        cosProjection += centered * std::cos(phase);
    }
    const double scale = 2.0 / static_cast<double>(samples.size());
    const double a = sinProjection * scale;
    const double b = cosProjection * scale;
    double residualSquares = 0.0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const double phase = omega * static_cast<double>(i);
        const double model = mean + a * std::sin(phase) + b * std::cos(phase);
        const double error = static_cast<double>(samples[i]) - model;
        residualSquares += error * error;
    }
    const double rms = std::sqrt(sumSquares / static_cast<double>(samples.size()));
    const double residualRms = std::sqrt(residualSquares / static_cast<double>(samples.size()));
    return {
        measuredFrequency,
        std::abs(measuredFrequency - expectedFrequency) / expectedFrequency,
        rms > 0.0 ? residualRms / rms : 0.0,
        mean,
        rms,
        peak,
    };
}

ToneMetrics renderTone(double playbackRate) {
    broke::Engine engine;
    engine.prepare(outputRate);
    engine.crossfader = 0.0f;
    engine.master = 1.0f;
    check(engine.submit(0, sineClip(1000.0)), "tone clip accepted");
    warmup(engine, blockSize);
    auto& control = engine.control(0);
    control.loop = true;
    control.rate = static_cast<float>(playbackRate);
    control.playing = true;
    warmup(engine);
    return analyseTone(render(engine, 48000), 1000.0 * playbackRate);
}

std::vector<float> renderRateConvertedTone(double sourceFrequency, double playbackRate) {
    broke::Engine engine;
    engine.prepare(outputRate);
    engine.crossfader = 0.0f;
    engine.master = 1.0f;
    check(engine.submit(0, sineClip(sourceFrequency)), "rate-conversion clip accepted");
    warmup(engine, blockSize);
    auto& control = engine.control(0);
    control.loop = true;
    control.rate = static_cast<float>(playbackRate);
    control.playing = true;
    warmup(engine);
    return render(engine, 48000);
}

float maxDelta(const std::vector<float>& samples, float previous) {
    float result = 0.0f;
    for (const float sample : samples) {
        result = std::max(result, std::abs(sample - previous));
        previous = sample;
    }
    return result;
}

void run() {
    const std::array<double, 3> rates{0.75, 1.0, 1.25};
    for (const double rate : rates) {
        const auto metrics = renderTone(rate);
        check(metrics.frequencyError < 0.005, "rate conversion frequency error stays below 0.5 percent");
        check(metrics.residualRatio < 0.03, "steady tone residual stays below 3 percent RMS");
        check(std::abs(metrics.dc) < 0.002, "steady tone DC stays bounded");
        check(metrics.rms > 0.05, "steady tone retains useful level");
        check(metrics.peak <= 0.98f + 1.0e-6f, "steady tone respects output bound");
        std::cout << std::fixed << std::setprecision(6)
                  << "METRIC tone rate=" << rate
                  << " measured_hz=" << metrics.measuredFrequency
                  << " frequency_error=" << metrics.frequencyError
                  << " residual_ratio=" << metrics.residualRatio
                  << " dc=" << metrics.dc
                  << " rms=" << metrics.rms
                  << " peak=" << metrics.peak << '\n';
    }

    // At 1.5x, an 8 kHz source remains below the output Nyquist limit (12 kHz),
    // while a 19 kHz source would fold to an audible alias without a low-pass
    // before decimation. The ratio makes this a deterministic spectral guard,
    // not a claim of perceptual transparency.
    const auto passband = renderRateConvertedTone(8000.0, 1.5);
    const auto stopband = renderRateConvertedTone(19000.0, 1.5);
    const double passbandRms = rmsOf(passband);
    const double stopbandRms = rmsOf(stopband);
    const double aliasRatio = passbandRms > 0.0 ? stopbandRms / passbandRms : 1.0;
    check(passbandRms > 0.08, "bandlimited resampler preserves useful 8 kHz passband level at 1.5x");
    check(stopbandRms < 0.02, "bandlimited resampler rejects out-of-band 19 kHz energy at 1.5x");
    check(aliasRatio < 0.10, "out-of-band RMS stays below ten percent of passband RMS");
    std::cout << std::fixed << std::setprecision(6)
              << "METRIC resampler_1_5x passband_8k_rms=" << passbandRms
              << " stopband_19k_rms=" << stopbandRms
              << " stopband_to_passband=" << aliasRatio << '\n';

    broke::Engine engine;
    engine.prepare(outputRate);
    engine.crossfader = 0.0f;
    engine.master = 1.0f;
    check(engine.submit(0, stepClip()), "transition clip accepted");
    warmup(engine, blockSize);
    auto& control = engine.control(0);
    control.playing = true;
    warmup(engine, 2048);
    auto before = render(engine, blockSize);
    const float stablePositive = before.back();
    check(stablePositive > 0.2f, "transition fixture reaches positive steady state");

    control.seek = 0.75;
    const auto seek = render(engine, 1024);
    const float seekDelta = maxDelta(seek, stablePositive);
    check(seekDelta < 0.05f, "seek transition bounds adjacent-sample discontinuity");
    check(seek.back() < -0.2f, "seek transition reaches negative destination");

    const float beforePause = seek.back();
    control.playing = false;
    const auto pause = render(engine, 1024);
    const float pauseDelta = maxDelta(pause, beforePause);
    check(pauseDelta < 0.05f, "pause transition bounds adjacent-sample discontinuity");
    check(std::abs(pause.back()) < 0.01f, "pause transition settles near silence");

    control.loop = true;
    control.seek = 0.99;
    control.playing = true;
    warmup(engine, 64);
    auto preLoop = render(engine, 16);
    const float beforeLoop = preLoop.back();
    const auto loop = render(engine, 1024);
    const float loopDelta = maxDelta(loop, beforeLoop);
    check(loopDelta < 0.05f, "loop transition bounds adjacent-sample discontinuity");
    check(control.playing.load(), "loop transition keeps transport active");

    std::cout << std::fixed << std::setprecision(6)
              << "METRIC transitions seek_max_delta=" << seekDelta
              << " pause_max_delta=" << pauseDelta
              << " loop_max_delta=" << loopDelta << '\n';
}
} // namespace

int main() {
    try {
        run();
        std::cout << "PASS: " << checks << " render metric checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
