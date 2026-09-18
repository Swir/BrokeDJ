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

std::unique_ptr<broke::Clip> stepClip(int frames = 4096) {
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
