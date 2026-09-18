// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "core/TimeStretchSourceBridge.h"

#include <array>
#include <cstddef>
#include <vector>

namespace broke {

// Opt-in research bridge from the source-rate time-stretch path to a fixed
// device-output sample rate. It remains separate from Engine::process(); the
// production pitch-changing resampler is still the normal playback path.
//
// prepare() owns every allocation and precomputes the band-limited resampler
// kernel. prime() is a discontinuity preparation point and is intentionally
// kept outside the realtime render contract. render() performs only bounded
// work and consumes caller-provided production fallback audio for deterministic
// enable/bypass/failure transitions.
class TimeStretchDeviceBridge final {
public:
    enum class RenderPath {
        fallback,
        stretch
    };

    enum class FallbackReason {
        none,
        disabled,
        unprimed,
        cursorDiscontinuity,
        clipChanged,
        loopModeChanged,
        sourceRateMismatch,
        stretchFailure
    };

    [[nodiscard]] bool prepare(double sourceSampleRate, double deviceSampleRate,
                               int maxDeviceFrames, double maxPlaybackRate = 4.0,
                               bool splitComputation = true);
    void reset() noexcept;

    [[nodiscard]] bool setPlaybackRate(double playbackRate) noexcept;
    [[nodiscard]] bool setPitchSemitones(float semitones) noexcept;
    void setEnabled(bool shouldEnable) noexcept;

    // Must be called after load/seek/loop-style discontinuities before the
    // stretch path can be selected again. It resets the source-rate FIFO and
    // fractional device resampler clock and binds the prepared state to this
    // exact immutable Clip plus loop mode. render() fails back if either changes
    // without an explicit off-callback prime().
    [[nodiscard]] bool prime(const Clip& clip, double cursor, bool loop) noexcept;

    // fallbackLeft/right and fallbackNextCursor describe the production path
    // for the same device block. If the research path is disabled, unprimed or
    // fails closed, render() emits that fallback and preserves its transport.
    // On a successful stretch block, nextCursor is advanced in source frames by
    // device duration * playbackRate, independent of bounded FIFO prefetch.
    [[nodiscard]] bool render(const Clip& clip, double cursor, bool loop,
                              const float* fallbackLeft, const float* fallbackRight,
                              double fallbackNextCursor,
                              float* outputLeft, float* outputRight,
                              int deviceFrames, double& nextCursor) noexcept;

    [[nodiscard]] bool prepared() const noexcept { return ready; }
    [[nodiscard]] bool primed() const noexcept { return primedForClip; }
    [[nodiscard]] bool needsPrime() const noexcept { return rePrimeRequired; }
    [[nodiscard]] RenderPath lastRenderPath() const noexcept { return lastPath; }
    [[nodiscard]] FallbackReason lastFallbackReason() const noexcept { return fallbackReason; }
    [[nodiscard]] double sourceSampleRate() const noexcept { return sourceRate; }
    [[nodiscard]] double deviceSampleRate() const noexcept { return deviceRate; }
    [[nodiscard]] double sourceFramesPerDeviceFrame() const noexcept { return sourcePerDevice; }
    [[nodiscard]] double playbackRateValue() const noexcept { return playbackRate; }
    [[nodiscard]] float pitchSemitonesValue() const noexcept { return pitchSemitones; }
    [[nodiscard]] int maxDeviceFrames() const noexcept { return maxDevice; }
    [[nodiscard]] int sourceInputLatencyFrames() const noexcept {
        return sourceBridge.inputLatencyFrames();
    }
    [[nodiscard]] int sourceOutputLatencyFrames() const noexcept {
        return sourceBridge.outputLatencyFrames();
    }
    // This is algorithm metadata for future scheduling, not measured hardware
    // latency. It combines reported stretch output latency with the finite SRC
    // history in the device-output domain.
    [[nodiscard]] int reportedDeviceOutputLatencyFrames() const noexcept;

private:
    static constexpr std::size_t taps = 24;
    static constexpr std::size_t phases = 128;
    static constexpr int firstTap = 1 - static_cast<int>(taps / 2);
    static constexpr int historyFrames = static_cast<int>(taps / 2);

    [[nodiscard]] bool fillSourceFifo(const Clip& clip, bool loop,
                                      std::size_t requiredCount) noexcept;
    [[nodiscard]] bool renderStretchBlock(const Clip& clip, bool loop,
                                          float* outputLeft, float* outputRight,
                                          int deviceFrames) noexcept;
    [[nodiscard]] double advanceCursor(const Clip& clip, double cursor,
                                       bool loop, int deviceFrames) const noexcept;
    void buildKernel();
    void clearFifo() noexcept;
    void invalidatePrime(FallbackReason reason) noexcept;
    void compactFifo(double nextReadPosition) noexcept;
    void renderFallback(const float* fallbackLeft, const float* fallbackRight,
                        float* outputLeft, float* outputRight, int deviceFrames) noexcept;
    void blendTransition(const float* fallbackLeft, const float* fallbackRight,
                         float* outputLeft, float* outputRight,
                         int deviceFrames, RenderPath targetPath) noexcept;

    TimeStretchSourceBridge sourceBridge;
    std::vector<float> sourceFifoLeft;
    std::vector<float> sourceFifoRight;
    std::vector<float> stretchScratchLeft;
    std::vector<float> stretchScratchRight;
    std::vector<float> kernels;
    std::array<float, 2> lastOutput{};
    std::array<float, 2> transitionFrom{};
    const Clip* primedClip = nullptr;
    double sourceRate = 0.0;
    double deviceRate = 0.0;
    double sourcePerDevice = 1.0;
    double playbackRate = 1.0;
    double sourceCursor = 0.0;
    double audibleCursor = 0.0;
    double readPosition = static_cast<double>(historyFrames);
    std::size_t fifoCount = 0;
    int maxDevice = 0;
    int maxStretchOutput = 0;
    int transitionFrames = 1;
    int transitionRemaining = 0;
    float pitchSemitones = 0.0f;
    bool ready = false;
    bool enabled = false;
    bool primedForClip = false;
    bool primedLoop = false;
    bool rePrimeRequired = true;
    RenderPath lastPath = RenderPath::fallback;
    FallbackReason fallbackReason = FallbackReason::unprimed;
};

} // namespace broke
