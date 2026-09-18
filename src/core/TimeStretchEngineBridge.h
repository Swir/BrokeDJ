// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "core/Engine.h"
#include "core/TimeStretchDeviceBridge.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

namespace broke {

// Opt-in M2 integration boundary around TimeStretchDeviceBridge. It is still
// not wired into Engine::process() and exposes no UI control. The class exists
// to qualify the exact callback contract Engine will need: render the current
// production-style hybrid source converter in parallel as deterministic
// fallback, feed that fallback to the bounded stretch/device bridge, preserve
// production transport on failure, and expose an algorithm-latency-compensated
// audible cursor for future deck/sync scheduling.
//
// prepare() is the only allocating operation. configureAndPrime()/prime() are
// off-callback discontinuity operations. render() is bounded and performs no
// resize, file I/O, decoding, logging or blocking synchronization.
class TimeStretchEngineBridge final {
public:
    using RenderPath = TimeStretchDeviceBridge::RenderPath;
    using FallbackReason = TimeStretchDeviceBridge::FallbackReason;

    struct ControlSnapshot final {
        double playbackRate = 1.0;
        float pitchSemitones = 0.0f;
        bool enabled = false;
    };

    [[nodiscard]] bool prepare(double sourceSampleRate, double deviceSampleRate,
                               int maxDeviceFrames, double maxPlaybackRate = 4.0,
                               bool splitComputation = true) {
        ready = false;
        if (!std::isfinite(deviceSampleRate) || deviceSampleRate < 8000.0
            || deviceSampleRate > 192000.0 || maxDeviceFrames <= 0
            || maxDeviceFrames > 8192) {
            return false;
        }
        if (!bridge.prepare(sourceSampleRate, deviceSampleRate, maxDeviceFrames,
                            maxPlaybackRate, splitComputation)) {
            return false;
        }
        outputRate = deviceSampleRate;
        maxDevice = maxDeviceFrames;
        playbackRate = 1.0;
        pitchSemitones = 0.0f;
        fallbackLeft.assign(static_cast<std::size_t>(maxDeviceFrames), 0.0f);
        fallbackRight.assign(static_cast<std::size_t>(maxDeviceFrames), 0.0f);
        kernels.assign(cutoffBins * phases * taps, 0.0f);
        buildKernels();
        ready = true;
        return true;
    }

    void reset() noexcept { bridge.reset(); }

    [[nodiscard]] bool setPlaybackRate(double rate) noexcept {
        if (!ready || !bridge.setPlaybackRate(rate)) return false;
        playbackRate = rate;
        return true;
    }

    [[nodiscard]] bool setPitchSemitones(float semitones) noexcept {
        if (!ready || !bridge.setPitchSemitones(semitones)) return false;
        pitchSemitones = semitones;
        return true;
    }

    void setEnabled(bool enabled) noexcept { bridge.setEnabled(enabled); }

    [[nodiscard]] bool prime(const Clip& clip, double cursor, bool loop) noexcept {
        return ready && bridge.prime(clip, cursor, loop);
    }

    // Transactional off-callback control/discontinuity handoff intended for the
    // future production Engine owner. Controls are validated before any state is
    // changed. A valid snapshot first disables and resets stale prefetched audio,
    // applies rate/pitch, then primes at the exact immutable Clip/cursor/loop
    // identity before enabling stretch. A disabled snapshot intentionally stays
    // unprimed so bypassed transport can never later resume stale FIFO content.
    // Invalid snapshots fail closed by disabling/resetting the bridge.
    [[nodiscard]] bool configureAndPrime(const Clip& clip, double cursor, bool loop,
                                         const ControlSnapshot& controls) noexcept {
        const bool validControls = std::isfinite(controls.playbackRate)
            && controls.playbackRate >= minPlaybackRate
            && controls.playbackRate <= maxPlaybackRate
            && std::isfinite(controls.pitchSemitones)
            && controls.pitchSemitones >= minPitchSemitones
            && controls.pitchSemitones <= maxPitchSemitones;
        if (!ready || !clip.valid() || !std::isfinite(cursor) || !validControls) {
            bridge.setEnabled(false);
            bridge.reset();
            return false;
        }

        bridge.setEnabled(false);
        bridge.reset();
        if (!bridge.setPlaybackRate(controls.playbackRate)
            || !bridge.setPitchSemitones(controls.pitchSemitones)) {
            bridge.reset();
            return false;
        }
        playbackRate = controls.playbackRate;
        pitchSemitones = controls.pitchSemitones;
        if (!controls.enabled) return true;
        if (!bridge.prime(clip, cursor, loop)) return false;
        bridge.setEnabled(true);
        return true;
    }

    // Renders one future Engine source block. The production-style fallback is
    // generated first and is therefore always available to the device bridge if
    // stretch is disabled, stale or fails closed. nextTransportCursor follows
    // whichever audible path won. nextAudibleCursor applies the research
    // algorithm latency only while stretch is actually selected; this is the
    // scheduling cursor a future meter/sync layer should consume. Hardware
    // latency is intentionally not included.
    [[nodiscard]] bool render(const Clip& clip, double cursor, bool loop,
                              float* outputLeft, float* outputRight,
                              int deviceFrames, double& nextTransportCursor,
                              double& nextAudibleCursor) noexcept {
        if (!ready || !clip.valid() || outputLeft == nullptr || outputRight == nullptr
            || deviceFrames <= 0 || deviceFrames > maxDevice || !std::isfinite(cursor)) {
            return false;
        }

        double fallbackNext = cursor;
        bool fallbackStreamAttempted = false;
        bool fallbackStreamReady = true;
        std::int64_t fallbackDiagnosticFrame = 0;
        renderProductionFallback(clip, cursor, loop, deviceFrames, fallbackNext,
                                 fallbackStreamAttempted, fallbackStreamReady,
                                 fallbackDiagnosticFrame);
        double next = cursor;
        if (!bridge.render(clip, cursor, loop,
                           fallbackLeft.data(), fallbackRight.data(), fallbackNext,
                           outputLeft, outputRight, deviceFrames, next)) {
            return false;
        }
        if (clip.stream && bridge.lastRenderPath() == RenderPath::fallback
            && fallbackStreamAttempted) {
            if (fallbackStreamReady) clip.stream->noteRefill();
            else clip.stream->noteStarvation(fallbackDiagnosticFrame);
        }
        nextTransportCursor = next;
        nextAudibleCursor = bridge.lastRenderPath() == RenderPath::stretch
            ? latencyCompensatedCursor(clip, next, loop)
            : normalizeCursor(clip, next, loop);
        return true;
    }

    [[nodiscard]] bool prepared() const noexcept { return ready && bridge.prepared(); }
    [[nodiscard]] bool primed() const noexcept { return bridge.primed(); }
    [[nodiscard]] bool needsPrime() const noexcept { return bridge.needsPrime(); }
    [[nodiscard]] RenderPath lastRenderPath() const noexcept { return bridge.lastRenderPath(); }
    [[nodiscard]] FallbackReason lastFallbackReason() const noexcept {
        return bridge.lastFallbackReason();
    }
    [[nodiscard]] int reportedDeviceOutputLatencyFrames() const noexcept {
        return bridge.reportedDeviceOutputLatencyFrames();
    }
    [[nodiscard]] double playbackRateValue() const noexcept { return playbackRate; }
    [[nodiscard]] float pitchSemitonesValue() const noexcept { return pitchSemitones; }
    [[nodiscard]] double deviceSampleRate() const noexcept { return outputRate; }

    [[nodiscard]] double latencyCompensatedCursor(const Clip& clip,
                                                  double transportCursor,
                                                  bool loop) const noexcept {
        if (!clip.valid() || !std::isfinite(transportCursor) || outputRate <= 0.0)
            return transportCursor;
        const double sourceFramesPerDeviceFrame = clip.sampleRate / outputRate;
        const double sourceLatency = static_cast<double>(reportedDeviceOutputLatencyFrames())
            * sourceFramesPerDeviceFrame * playbackRate;
        return normalizeCursor(clip, transportCursor - sourceLatency, loop);
    }

private:
    struct ReadPoint final {
        float value = 0.0f;
        bool ready = false;
    };

    static constexpr std::size_t taps = 24;
    static constexpr std::size_t phases = 128;
    static constexpr std::size_t cutoffBins = 64;
    static constexpr float minCutoff = 0.06f;
    static constexpr double minPlaybackRate = 0.25;
    static constexpr double maxPlaybackRate = 4.0;
    static constexpr float minPitchSemitones = -24.0f;
    static constexpr float maxPitchSemitones = 24.0f;

    [[nodiscard]] static float finite(float value) noexcept {
        return std::isfinite(value) ? value : 0.0f;
    }

    [[nodiscard]] static ReadPoint point(const Clip& clip, int channel,
                                         std::int64_t index, bool loop) noexcept {
        const auto size = clip.frames();
        if (size <= 0) return {};
        if (loop) {
            index %= size;
            if (index < 0) index += size;
        } else {
            index = std::clamp<std::int64_t>(index, 0, size - 1);
        }
        if (clip.stream) {
            float value = 0.0f;
            const bool sampleReady = clip.stream->trySample(channel, index, value);
            return {finite(value), sampleReady};
        }
        const auto& data = channel == 0 ? clip.left : clip.right;
        return {finite(data[static_cast<std::size_t>(index)]), true};
    }

    [[nodiscard]] const float* kernel(double step, double cursor) const noexcept {
        if (kernels.empty() || !std::isfinite(step) || step <= 1.0001
            || !std::isfinite(cursor)) {
            return nullptr;
        }
        constexpr double guard = 0.985;
        const double desiredCutoff = std::clamp(guard / step,
            static_cast<double>(minCutoff), 1.0);
        const double cutoffPosition = (desiredCutoff - static_cast<double>(minCutoff))
            / (1.0 - static_cast<double>(minCutoff));
        const auto bin = std::min<std::size_t>(cutoffBins - 1,
            static_cast<std::size_t>(std::floor(cutoffPosition
                * static_cast<double>(cutoffBins - 1))));
        const double fraction = cursor - std::floor(cursor);
        const auto phase = std::min<std::size_t>(phases - 1,
            static_cast<std::size_t>(fraction * static_cast<double>(phases)));
        return kernels.data() + (bin * phases + phase) * taps;
    }

    [[nodiscard]] ReadPoint resample(const Clip& clip, int channel, double cursor,
                                     bool loop, const float* bandlimitedKernel) const noexcept {
        if (clip.frames() <= 0 || !std::isfinite(cursor)) return {};
        if (bandlimitedKernel != nullptr
            && clip.frames() >= static_cast<std::int64_t>(taps)) {
            constexpr int firstTap = 1 - static_cast<int>(taps / 2);
            const auto centre = static_cast<std::int64_t>(std::floor(cursor));
            float value = 0.0f;
            for (std::size_t tap = 0; tap < taps; ++tap) {
                const auto sample = point(clip, channel,
                    centre + static_cast<std::int64_t>(firstTap + static_cast<int>(tap)), loop);
                if (!sample.ready) return {};
                value += sample.value * bandlimitedKernel[tap];
            }
            return {finite(value), true};
        }
        if (clip.frames() < 4) {
            const auto i = static_cast<std::int64_t>(std::floor(cursor));
            const auto fraction = static_cast<float>(cursor - static_cast<double>(i));
            const auto a = point(clip, channel, i, loop);
            const auto b = point(clip, channel, i + 1, loop);
            if (!a.ready || !b.ready) return {};
            return {finite(a.value + (b.value - a.value) * fraction), true};
        }
        const auto i = static_cast<std::int64_t>(std::floor(cursor));
        const float fraction = static_cast<float>(cursor - static_cast<double>(i));
        const auto p0 = point(clip, channel, i - 1, loop);
        const auto p1 = point(clip, channel, i, loop);
        const auto p2 = point(clip, channel, i + 1, loop);
        const auto p3 = point(clip, channel, i + 2, loop);
        if (!p0.ready || !p1.ready || !p2.ready || !p3.ready) return {};
        const float t2 = fraction * fraction;
        const float t3 = t2 * fraction;
        return {finite(0.5f * ((2.0f * p1.value) + (-p0.value + p2.value) * fraction
            + (2.0f * p0.value - 5.0f * p1.value + 4.0f * p2.value - p3.value) * t2
            + (-p0.value + 3.0f * p1.value - 3.0f * p2.value + p3.value) * t3)), true};
    }

    void renderProductionFallback(const Clip& clip, double cursor, bool loop,
                                  int deviceFrames, double& nextCursor,
                                  bool& streamAttempted, bool& streamReady,
                                  std::int64_t& diagnosticFrame) noexcept {
        const double length = static_cast<double>(clip.frames());
        const double step = clip.sampleRate / outputRate * playbackRate;
        double position = normalizeCursor(clip, cursor, loop);
        streamAttempted = false;
        streamReady = true;
        diagnosticFrame = static_cast<std::int64_t>(std::clamp(
            position, 0.0, static_cast<double>(std::max<std::int64_t>(0, clip.frames() - 1))));
        for (int frame = 0; frame < deviceFrames; ++frame) {
            if (!loop && position >= length) {
                fallbackLeft[static_cast<std::size_t>(frame)] = 0.0f;
                fallbackRight[static_cast<std::size_t>(frame)] = 0.0f;
                continue;
            }
            const auto* selectedKernel = kernel(step, position);
            const auto left = resample(clip, 0, position, loop, selectedKernel);
            const auto right = resample(clip, 1, position, loop, selectedKernel);
            fallbackLeft[static_cast<std::size_t>(frame)] = left.value;
            fallbackRight[static_cast<std::size_t>(frame)] = right.value;
            if (clip.stream) {
                streamAttempted = true;
                if (!left.ready || !right.ready) {
                    streamReady = false;
                    diagnosticFrame = static_cast<std::int64_t>(std::clamp(
                        position, 0.0,
                        static_cast<double>(std::max<std::int64_t>(0, clip.frames() - 1))));
                }
            }
            position += step;
            position = normalizeCursor(clip, position, loop);
        }
        nextCursor = position;
    }

    [[nodiscard]] static double normalizeCursor(const Clip& clip, double cursor,
                                                bool loop) noexcept {
        const double length = static_cast<double>(clip.frames());
        if (length <= 0.0 || !std::isfinite(cursor)) return cursor;
        if (loop) {
            double value = std::fmod(cursor, length);
            if (value < 0.0) value += length;
            return value;
        }
        return std::clamp(cursor, 0.0, length);
    }

    void buildKernels() {
        constexpr int firstTap = 1 - static_cast<int>(taps / 2);
        const double radius = static_cast<double>(taps) * 0.5;
        const double cutoffSpan = 1.0 - static_cast<double>(minCutoff);
        for (std::size_t bin = 0; bin < cutoffBins; ++bin) {
            const double cutoff = static_cast<double>(minCutoff)
                + cutoffSpan * static_cast<double>(bin) / static_cast<double>(cutoffBins - 1);
            for (std::size_t phase = 0; phase < phases; ++phase) {
                const double fraction = static_cast<double>(phase) / static_cast<double>(phases);
                const auto base = (bin * phases + phase) * taps;
                double sum = 0.0;
                for (std::size_t tap = 0; tap < taps; ++tap) {
                    const double x = static_cast<double>(firstTap + static_cast<int>(tap)) - fraction;
                    const double distance = std::abs(x);
                    double weight = 0.0;
                    if (distance < radius) {
                        const double window = 0.42
                            + 0.5 * std::cos(std::numbers::pi * x / radius)
                            + 0.08 * std::cos(2.0 * std::numbers::pi * x / radius);
                        const double argument = cutoff * x;
                        const double sinc = std::abs(argument) < 1.0e-12
                            ? 1.0
                            : std::sin(std::numbers::pi * argument)
                                / (std::numbers::pi * argument);
                        weight = cutoff * sinc * window;
                    }
                    kernels[base + tap] = static_cast<float>(weight);
                    sum += weight;
                }
                if (std::abs(sum) > 1.0e-12) {
                    const float scale = static_cast<float>(1.0 / sum);
                    for (std::size_t tap = 0; tap < taps; ++tap)
                        kernels[base + tap] *= scale;
                }
            }
        }
    }

    TimeStretchDeviceBridge bridge;
    std::vector<float> fallbackLeft;
    std::vector<float> fallbackRight;
    std::vector<float> kernels;
    double outputRate = 0.0;
    double playbackRate = 1.0;
    float pitchSemitones = 0.0f;
    int maxDevice = 0;
    bool ready = false;
};

} // namespace broke
