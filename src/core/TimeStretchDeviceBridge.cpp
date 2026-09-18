// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#include "core/TimeStretchDeviceBridge.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>

namespace broke {
namespace {
constexpr double minRate = 8000.0;
constexpr double maxSourceRate = 384000.0;
constexpr double maxDeviceRate = 192000.0;
constexpr double minSampleRateRatio = 0.25;
constexpr double maxSampleRateRatio = 4.0;
constexpr int maxDeviceBlock = 8192;

float finiteSample(float value) noexcept {
    return std::isfinite(value) ? value : 0.0f;
}
} // namespace

bool TimeStretchDeviceBridge::prepare(double newSourceSampleRate, double newDeviceSampleRate,
                                      int newMaxDeviceFrames, double maxPlaybackRate,
                                      bool splitComputation) {
    ready = false;
    primedForClip = false;
    rePrimeRequired = true;
    if (!std::isfinite(newSourceSampleRate) || newSourceSampleRate < minRate
        || newSourceSampleRate > maxSourceRate || !std::isfinite(newDeviceSampleRate)
        || newDeviceSampleRate < minRate || newDeviceSampleRate > maxDeviceRate
        || newMaxDeviceFrames <= 0 || newMaxDeviceFrames > maxDeviceBlock) {
        return false;
    }

    const double ratio = newSourceSampleRate / newDeviceSampleRate;
    if (!std::isfinite(ratio) || ratio < minSampleRateRatio || ratio > maxSampleRateRatio) {
        return false;
    }

    const double requiredStretch = std::ceil(static_cast<double>(newMaxDeviceFrames) * ratio)
        + static_cast<double>(taps * 2 + 8);
    if (!std::isfinite(requiredStretch) || requiredStretch <= 0.0 || requiredStretch > 65536.0) {
        return false;
    }
    const int newMaxStretchOutput = static_cast<int>(requiredStretch);
    if (!sourceBridge.prepare(newSourceSampleRate, newMaxStretchOutput,
                              maxPlaybackRate, splitComputation)) {
        return false;
    }

    sourceRate = newSourceSampleRate;
    deviceRate = newDeviceSampleRate;
    sourcePerDevice = ratio;
    maxDevice = newMaxDeviceFrames;
    maxStretchOutput = newMaxStretchOutput;
    const auto fifoCapacity = static_cast<std::size_t>(historyFrames + maxStretchOutput + taps + 8);
    sourceFifoLeft.assign(fifoCapacity, 0.0f);
    sourceFifoRight.assign(fifoCapacity, 0.0f);
    stretchScratchLeft.assign(static_cast<std::size_t>(maxStretchOutput), 0.0f);
    stretchScratchRight.assign(static_cast<std::size_t>(maxStretchOutput), 0.0f);
    kernels.assign(phases * taps, 0.0f);
    buildKernel();

    transitionFrames = std::max(1, static_cast<int>(std::lround(deviceRate * 0.005)));
    playbackRate = 1.0;
    enabled = false;
    lastPath = RenderPath::fallback;
    lastOutput.fill(0.0f);
    clearFifo();
    ready = true;
    return true;
}

void TimeStretchDeviceBridge::reset() noexcept {
    sourceBridge.reset();
    sourceCursor = 0.0;
    audibleCursor = 0.0;
    transitionRemaining = 0;
    lastOutput.fill(0.0f);
    lastPath = RenderPath::fallback;
    primedForClip = false;
    rePrimeRequired = true;
    clearFifo();
}

bool TimeStretchDeviceBridge::setPlaybackRate(double newPlaybackRate) noexcept {
    if (!ready || !sourceBridge.setPlaybackRate(newPlaybackRate)) return false;
    playbackRate = newPlaybackRate;
    return true;
}

bool TimeStretchDeviceBridge::setPitchSemitones(float semitones) noexcept {
    return ready && sourceBridge.setPitchSemitones(semitones);
}

void TimeStretchDeviceBridge::buildKernel() {
    const double cutoff = std::min(1.0, 0.985 / sourcePerDevice);
    const double radius = static_cast<double>(taps) * 0.5;
    for (std::size_t phase = 0; phase < phases; ++phase) {
        const double fraction = static_cast<double>(phase) / static_cast<double>(phases);
        double sum = 0.0;
        const auto base = phase * taps;
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
                    : std::sin(std::numbers::pi * argument) / (std::numbers::pi * argument);
                weight = cutoff * sinc * window;
            }
            kernels[base + tap] = static_cast<float>(weight);
            sum += weight;
        }
        if (std::abs(sum) > 1.0e-12) {
            const float scale = static_cast<float>(1.0 / sum);
            for (std::size_t tap = 0; tap < taps; ++tap) kernels[base + tap] *= scale;
        }
    }
}

void TimeStretchDeviceBridge::clearFifo() noexcept {
    fifoCount = static_cast<std::size_t>(historyFrames);
    readPosition = static_cast<double>(historyFrames);
    const auto zeroCount = std::min(fifoCount, sourceFifoLeft.size());
    if (zeroCount > 0) {
        std::fill_n(sourceFifoLeft.begin(), static_cast<std::ptrdiff_t>(zeroCount), 0.0f);
        std::fill_n(sourceFifoRight.begin(), static_cast<std::ptrdiff_t>(zeroCount), 0.0f);
    }
}

bool TimeStretchDeviceBridge::prime(const Clip& clip, double cursor, bool loop) noexcept {
    primedForClip = false;
    rePrimeRequired = true;
    transitionRemaining = 0;
    if (!ready || !std::isfinite(cursor) || !sourceBridge.prime(clip, cursor, loop)) return false;
    sourceCursor = cursor;
    audibleCursor = cursor;
    clearFifo();
    primedForClip = true;
    rePrimeRequired = false;
    return true;
}

bool TimeStretchDeviceBridge::fillSourceFifo(const Clip& clip, bool loop,
                                             std::size_t requiredCount) noexcept {
    if (requiredCount <= fifoCount) return true;
    if (requiredCount > sourceFifoLeft.size()) return false;
    std::size_t needed = requiredCount - fifoCount;
    // Very slow playback can map a tiny stretch-output request to less than one
    // source input frame. Request a small bounded surplus and retain it in the
    // FIFO instead of violating TimeStretchDeckAdapter's exact-input contract.
    needed = std::max<std::size_t>(needed, 4);
    if (fifoCount + needed > sourceFifoLeft.size()
        || needed > static_cast<std::size_t>(maxStretchOutput)) {
        return false;
    }

    double nextSourceCursor = sourceCursor;
    if (!sourceBridge.render(clip, sourceCursor, loop,
                             stretchScratchLeft.data(), stretchScratchRight.data(),
                             static_cast<int>(needed), nextSourceCursor)) {
        return false;
    }
    std::copy_n(stretchScratchLeft.data(), static_cast<std::ptrdiff_t>(needed),
                sourceFifoLeft.data() + fifoCount);
    std::copy_n(stretchScratchRight.data(), static_cast<std::ptrdiff_t>(needed),
                sourceFifoRight.data() + fifoCount);
    fifoCount += needed;
    sourceCursor = nextSourceCursor;
    return true;
}

void TimeStretchDeviceBridge::compactFifo(double nextReadPosition) noexcept {
    const auto integerPosition = static_cast<std::size_t>(std::max(0.0, std::floor(nextReadPosition)));
    const std::size_t keepHistory = static_cast<std::size_t>(historyFrames);
    const std::size_t consume = integerPosition > keepHistory ? integerPosition - keepHistory : 0;
    if (consume > 0 && consume < fifoCount) {
        const std::size_t remaining = fifoCount - consume;
        std::memmove(sourceFifoLeft.data(), sourceFifoLeft.data() + consume, remaining * sizeof(float));
        std::memmove(sourceFifoRight.data(), sourceFifoRight.data() + consume, remaining * sizeof(float));
        fifoCount = remaining;
        readPosition = nextReadPosition - static_cast<double>(consume);
    } else if (consume >= fifoCount) {
        clearFifo();
    } else {
        readPosition = nextReadPosition;
    }
}

bool TimeStretchDeviceBridge::renderStretchBlock(const Clip& clip, bool loop,
                                                 float* outputLeft, float* outputRight,
                                                 int deviceFrames) noexcept {
    if (!primedForClip || rePrimeRequired || deviceFrames <= 0 || deviceFrames > maxDevice
        || outputLeft == nullptr || outputRight == nullptr || kernels.empty()) {
        return false;
    }

    const double lastPosition = readPosition
        + static_cast<double>(deviceFrames - 1) * sourcePerDevice;
    const auto lastCentre = static_cast<std::size_t>(std::max(0.0, std::floor(lastPosition)));
    const std::size_t requiredCount = lastCentre + static_cast<std::size_t>(historyFrames) + 1;
    if (!fillSourceFifo(clip, loop, requiredCount)) return false;

    for (int frame = 0; frame < deviceFrames; ++frame) {
        const double position = readPosition + static_cast<double>(frame) * sourcePerDevice;
        const auto centre = static_cast<std::int64_t>(std::floor(position));
        const double fraction = position - static_cast<double>(centre);
        const auto phase = std::min<std::size_t>(phases - 1,
            static_cast<std::size_t>(fraction * static_cast<double>(phases)));
        const auto kernelBase = phase * taps;
        float left = 0.0f;
        float right = 0.0f;
        for (std::size_t tap = 0; tap < taps; ++tap) {
            const auto index = centre + static_cast<std::int64_t>(firstTap + static_cast<int>(tap));
            if (index < 0 || static_cast<std::size_t>(index) >= fifoCount) return false;
            const float weight = kernels[kernelBase + tap];
            left += sourceFifoLeft[static_cast<std::size_t>(index)] * weight;
            right += sourceFifoRight[static_cast<std::size_t>(index)] * weight;
        }
        outputLeft[frame] = finiteSample(left);
        outputRight[frame] = finiteSample(right);
    }

    compactFifo(readPosition + static_cast<double>(deviceFrames) * sourcePerDevice);
    return true;
}

double TimeStretchDeviceBridge::advanceCursor(const Clip& clip, double cursor,
                                              bool loop, int deviceFrames) const noexcept {
    const double length = static_cast<double>(clip.frames());
    if (length <= 0.0 || !std::isfinite(cursor)) return cursor;
    double next = cursor + static_cast<double>(deviceFrames) * sourcePerDevice * playbackRate;
    if (loop) {
        next = std::fmod(next, length);
        if (next < 0.0) next += length;
        return next;
    }
    return std::clamp(next, 0.0, length);
}

void TimeStretchDeviceBridge::renderFallback(const float* fallbackLeft, const float* fallbackRight,
                                             float* outputLeft, float* outputRight,
                                             int deviceFrames) noexcept {
    for (int frame = 0; frame < deviceFrames; ++frame) {
        outputLeft[frame] = finiteSample(fallbackLeft[frame]);
        outputRight[frame] = finiteSample(fallbackRight[frame]);
    }
}

void TimeStretchDeviceBridge::blendTransition(const float* fallbackLeft, const float* fallbackRight,
                                              float* outputLeft, float* outputRight,
                                              int deviceFrames, RenderPath targetPath) noexcept {
    if (transitionRemaining <= 0) return;
    for (int frame = 0; frame < deviceFrames && transitionRemaining > 0; ++frame) {
        const float progress = 1.0f
            - static_cast<float>(transitionRemaining) / static_cast<float>(transitionFrames);
        if (targetPath == RenderPath::stretch) {
            const float fallbackL = finiteSample(fallbackLeft[frame]);
            const float fallbackR = finiteSample(fallbackRight[frame]);
            outputLeft[frame] = fallbackL * (1.0f - progress) + outputLeft[frame] * progress;
            outputRight[frame] = fallbackR * (1.0f - progress) + outputRight[frame] * progress;
        } else {
            outputLeft[frame] = lastOutput[0] * (1.0f - progress) + outputLeft[frame] * progress;
            outputRight[frame] = lastOutput[1] * (1.0f - progress) + outputRight[frame] * progress;
        }
        --transitionRemaining;
    }
}

bool TimeStretchDeviceBridge::render(const Clip& clip, double cursor, bool loop,
                                     const float* fallbackLeft, const float* fallbackRight,
                                     double fallbackNextCursor,
                                     float* outputLeft, float* outputRight,
                                     int deviceFrames, double& nextCursor) noexcept {
    if (!ready || fallbackLeft == nullptr || fallbackRight == nullptr
        || outputLeft == nullptr || outputRight == nullptr
        || deviceFrames <= 0 || deviceFrames > maxDevice
        || !std::isfinite(cursor) || !std::isfinite(fallbackNextCursor)) {
        return false;
    }

    constexpr double cursorTolerance = 1.0e-5;
    if (primedForClip && std::abs(cursor - audibleCursor) > cursorTolerance) {
        primedForClip = false;
        rePrimeRequired = true;
        sourceBridge.reset();
        clearFifo();
    }

    RenderPath target = RenderPath::fallback;
    bool stretchRendered = false;
    if (enabled && primedForClip && !rePrimeRequired) {
        stretchRendered = renderStretchBlock(clip, loop, outputLeft, outputRight, deviceFrames);
        if (stretchRendered) {
            target = RenderPath::stretch;
        } else {
            primedForClip = false;
            rePrimeRequired = true;
            sourceBridge.reset();
            clearFifo();
        }
    }

    if (!stretchRendered) renderFallback(fallbackLeft, fallbackRight, outputLeft, outputRight, deviceFrames);
    if (target != lastPath) transitionRemaining = transitionFrames;
    blendTransition(fallbackLeft, fallbackRight, outputLeft, outputRight, deviceFrames, target);

    nextCursor = target == RenderPath::stretch
        ? advanceCursor(clip, cursor, loop, deviceFrames)
        : fallbackNextCursor;
    audibleCursor = nextCursor;
    lastPath = target;
    lastOutput[0] = finiteSample(outputLeft[deviceFrames - 1]);
    lastOutput[1] = finiteSample(outputRight[deviceFrames - 1]);
    return true;
}

int TimeStretchDeviceBridge::reportedDeviceOutputLatencyFrames() const noexcept {
    if (!ready || sourcePerDevice <= 0.0) return 0;
    const double sourceDomainLatency = static_cast<double>(sourceBridge.outputLatencyFrames())
        + static_cast<double>(historyFrames);
    return std::max(0, static_cast<int>(std::ceil(sourceDomainLatency / sourcePerDevice)));
}

} // namespace broke
