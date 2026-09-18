// SPDX-License-Identifier: AGPL-3.0-only
#include "core/TimeStretchDeviceBridge.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace {
int checks = 0;

void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

broke::Clip makeTone(double sampleRate, double frequency, int seconds = 12) {
    broke::Clip clip;
    clip.sampleRate = sampleRate;
    const auto frames = static_cast<std::size_t>(sampleRate * static_cast<double>(seconds));
    clip.left.resize(frames);
    clip.right.resize(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const double phase = 2.0 * std::numbers::pi * frequency
            * static_cast<double>(i) / sampleRate;
        const float value = static_cast<float>(0.18 * std::sin(phase));
        clip.left[i] = value;
        clip.right[i] = value * 0.8f;
    }
    return clip;
}

double estimateFrequency(const std::vector<float>& samples, double sampleRate) {
    std::size_t crossings = 0;
    for (std::size_t i = 1; i < samples.size(); ++i)
        if (samples[i - 1] <= 0.0f && samples[i] > 0.0f) ++crossings;
    const double duration = samples.size() > 1
        ? static_cast<double>(samples.size() - 1) / sampleRate : 0.0;
    return duration > 0.0 ? static_cast<double>(crossings) / duration : 0.0;
}

double rms(const std::vector<float>& samples) {
    double sum = 0.0;
    for (float value : samples) sum += static_cast<double>(value) * static_cast<double>(value);
    return samples.empty() ? 0.0 : std::sqrt(sum / static_cast<double>(samples.size()));
}

std::vector<float> renderTone(double sourceRate, double deviceRate, double frequency,
                              double playbackRate, int blocks = 90) {
    constexpr int frames = 1024;
    auto clip = makeTone(sourceRate, frequency, 20);
    broke::TimeStretchDeviceBridge bridge;
    check(bridge.prepare(sourceRate, deviceRate, 4096, 4.0), "device bridge prepares");
    check(bridge.setPlaybackRate(playbackRate), "device bridge accepts playback rate");
    check(bridge.setPitchSemitones(0.0f), "device bridge accepts neutral pitch");
    bridge.setEnabled(true);
    double cursor = sourceRate + 0.5;
    check(bridge.prime(clip, cursor, false), "device bridge primes real source");

    std::vector<float> fallbackLeft(frames, 0.0f);
    std::vector<float> fallbackRight(frames, 0.0f);
    std::vector<float> left(frames);
    std::vector<float> right(frames);
    std::vector<float> captured;
    for (int block = 0; block < blocks; ++block) {
        const double expectedAdvance = static_cast<double>(frames)
            * (sourceRate / deviceRate) * playbackRate;
        const double fallbackNext = cursor + expectedAdvance;
        double next = cursor;
        check(bridge.render(clip, cursor, false,
                            fallbackLeft.data(), fallbackRight.data(), fallbackNext,
                            left.data(), right.data(), frames, next),
              "device bridge block renders");
        check(bridge.lastRenderPath() == broke::TimeStretchDeviceBridge::RenderPath::stretch,
              "primed enabled bridge selects stretch path");
        check(std::abs(next - fallbackNext) < 1.0e-6,
              "audible cursor follows device duration and playback rate");
        cursor = next;
        if (block >= 30) captured.insert(captured.end(), left.begin(), left.end());
    }
    return captured;
}

void run() {
    broke::TimeStretchDeviceBridge invalid;
    check(!invalid.prepare(44100.0, 0.0, 1024), "invalid device rate rejected");
    check(!invalid.prepare(384000.0, 48000.0, 1024),
          "unqualified extreme source/device ratio rejected");

    const auto converted = renderTone(44100.0, 48000.0, 440.0, 1.25);
    const double convertedHz = estimateFrequency(converted, 48000.0);
    check(std::abs(convertedHz - 440.0) < 6.0,
          "44.1-to-48 kHz 1.25x device bridge preserves pitch");
    check(std::all_of(converted.begin(), converted.end(),
                      [](float value) { return std::isfinite(value); }),
          "sample-rate converted output remains finite");

    const auto passband = renderTone(96000.0, 48000.0, 8000.0, 1.0, 70);
    const auto stopband = renderTone(96000.0, 48000.0, 30000.0, 1.0, 70);
    const double passbandRms = rms(passband);
    const double stopbandRms = rms(stopband);
    check(passbandRms > 0.04, "96-to-48 kHz passband retains useful level");
    check(stopbandRms < 0.04, "96-to-48 kHz out-of-band source is attenuated");
    check(stopbandRms / passbandRms < 0.30,
          "device bridge band-limited SRC suppresses alias-prone energy");

    constexpr int frames = 1024;
    auto clip = makeTone(48000.0, 330.0, 8);
    broke::TimeStretchDeviceBridge bridge;
    check(bridge.prepare(48000.0, 48000.0, 4096), "fallback fixture prepares");
    check(bridge.setPlaybackRate(1.0), "fallback fixture neutral rate accepted");
    check(bridge.setPitchSemitones(0.0f), "fallback fixture neutral pitch accepted");
    bridge.setEnabled(true);
    double cursor = 48000.25;
    check(bridge.prime(clip, cursor, false), "fallback fixture primes");

    std::vector<float> fallbackLeft(frames, 0.03125f);
    std::vector<float> fallbackRight(frames, -0.025f);
    std::vector<float> left(frames);
    std::vector<float> right(frames);
    double next = cursor;
    check(bridge.render(clip, cursor, false,
                        fallbackLeft.data(), fallbackRight.data(), cursor + frames,
                        left.data(), right.data(), frames, next),
          "initial stretch fixture renders");
    cursor = next;

    bridge.setEnabled(false);
    const double fallbackNext = cursor + 777.0;
    check(bridge.render(clip, cursor, false,
                        fallbackLeft.data(), fallbackRight.data(), fallbackNext,
                        left.data(), right.data(), frames, next),
          "explicit bypass renders supplied fallback");
    check(bridge.lastRenderPath() == broke::TimeStretchDeviceBridge::RenderPath::fallback,
          "disabled bridge reports fallback path");
    check(next == fallbackNext, "fallback path preserves production transport decision");
    check(std::abs(left.back() - fallbackLeft.back()) < 1.0e-6f
              && std::abs(right.back() - fallbackRight.back()) < 1.0e-6f,
          "fallback transition settles to production samples inside bounded fade");
    check(std::all_of(left.begin(), left.end(), [](float value) { return std::isfinite(value); }),
          "fallback transition remains finite");

    bridge.setEnabled(true);
    double discontinuous = next + 500.0;
    const double discontinuousFallback = discontinuous + 100.0;
    check(bridge.render(clip, discontinuous, false,
                        fallbackLeft.data(), fallbackRight.data(), discontinuousFallback,
                        left.data(), right.data(), frames, next),
          "unprimed/discontinuous enable fails safely to production path");
    check(bridge.lastRenderPath() == broke::TimeStretchDeviceBridge::RenderPath::fallback,
          "discontinuity never emits stale stretch FIFO");
    check(bridge.needsPrime(), "discontinuity requires explicit re-prime before stretch resumes");
    check(next == discontinuousFallback, "discontinuity fallback preserves caller transport");

    check(bridge.reportedDeviceOutputLatencyFrames() > 0,
          "device bridge exposes deterministic algorithm-latency metadata");

    std::cout << "METRIC device_bridge_44k1_to_48k_keylock_hz=" << convertedHz
              << " passband_rms=" << passbandRms
              << " stopband_rms=" << stopbandRms
              << " latency_frames=" << bridge.reportedDeviceOutputLatencyFrames() << '\n';
}
} // namespace

int main() {
    try {
        run();
        std::cout << "PASS: " << checks << " time-stretch device-bridge checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
