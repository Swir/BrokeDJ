// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#include "core/TimeStretchDeck.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace broke {
namespace {
constexpr double minPlaybackRate = 0.25;
constexpr double absoluteMaxPlaybackRate = 4.0;
constexpr int absoluteFrameBound = 65536;
}

bool TimeStretchDeckAdapter::prepare(double newSampleRate, int newMaxOutputFrames,
                                     double newMaxPlaybackRate, bool splitComputation) {
    if (!std::isfinite(newSampleRate) || newSampleRate < 8000.0 || newSampleRate > 384000.0
        || newMaxOutputFrames <= 0 || newMaxOutputFrames > absoluteFrameBound
        || !std::isfinite(newMaxPlaybackRate)
        || newMaxPlaybackRate < minPlaybackRate
        || newMaxPlaybackRate > absoluteMaxPlaybackRate) {
        return false;
    }

    const double worstInput = std::ceil(static_cast<double>(newMaxOutputFrames)
                                        * newMaxPlaybackRate) + 1.0;
    if (!std::isfinite(worstInput) || worstInput > static_cast<double>(absoluteFrameBound)) {
        return false;
    }
    const int newMaxInputFrames = static_cast<int>(worstInput);
    if (!processor.prepare(newSampleRate, newMaxInputFrames, newMaxOutputFrames, splitComputation)) {
        return false;
    }

    sampleRate = newSampleRate;
    maxRate = newMaxPlaybackRate;
    maxInput = newMaxInputFrames;
    maxOutput = newMaxOutputFrames;
    rate = std::clamp(rate, minPlaybackRate, maxRate);
    carry = 0.0;
    consumed = 0;
    ready = true;
    return true;
}

void TimeStretchDeckAdapter::reset() noexcept {
    if (ready) processor.reset();
    carry = 0.0;
    consumed = 0;
}

bool TimeStretchDeckAdapter::setPlaybackRate(double playbackRate) noexcept {
    if (!ready || !std::isfinite(playbackRate)
        || playbackRate < minPlaybackRate || playbackRate > maxRate) {
        return false;
    }
    rate = playbackRate;
    return true;
}

bool TimeStretchDeckAdapter::setPitchSemitones(float semitones) noexcept {
    return ready && processor.setPitchSemitones(semitones);
}

bool TimeStretchDeckAdapter::validOutputRequest(int outputFrames) const noexcept {
    return ready && outputFrames > 0 && outputFrames <= maxOutput;
}

int TimeStretchDeckAdapter::inputFramesForOutput(int outputFrames) const noexcept {
    if (!validOutputRequest(outputFrames)) return 0;
    const double exact = carry + static_cast<double>(outputFrames) * rate;
    if (!std::isfinite(exact) || exact < 1.0) return 0;
    const double floored = std::floor(exact);
    if (floored < 1.0 || floored > static_cast<double>(maxInput)) return 0;
    return static_cast<int>(floored);
}

bool TimeStretchDeckAdapter::processStereo(const float* left, const float* right, int inputFrames,
                                           float* outputLeft, float* outputRight,
                                           int outputFrames) noexcept {
    const int expectedInput = inputFramesForOutput(outputFrames);
    if (expectedInput <= 0 || inputFrames != expectedInput
        || left == nullptr || right == nullptr || outputLeft == nullptr || outputRight == nullptr) {
        return false;
    }

    const double exact = carry + static_cast<double>(outputFrames) * rate;
    if (!processor.processStereo(left, right, inputFrames, outputLeft, outputRight, outputFrames)) {
        return false;
    }

    carry = exact - static_cast<double>(inputFrames);
    if (carry < 0.0 && carry > -1.0e-12) carry = 0.0;
    if (carry >= 1.0 && carry < 1.0 + 1.0e-12) carry = 0.0;
    consumed += static_cast<std::int64_t>(inputFrames);
    return true;
}

bool TimeStretchDeckAdapter::primeAfterDiscontinuity(const float* left, const float* right,
                                                     int inputFrames) noexcept {
    if (!ready || left == nullptr || right == nullptr || inputFrames <= 0
        || inputFrames > maxInput) {
        return false;
    }
    processor.reset();
    carry = 0.0;
    consumed = 0;
    return processor.primeAfterSeek(left, right, inputFrames, rate);
}

int TimeStretchDeckAdapter::inputLatencyFrames() const noexcept {
    return ready ? processor.inputLatencyFrames() : 0;
}

int TimeStretchDeckAdapter::outputLatencyFrames() const noexcept {
    return ready ? processor.outputLatencyFrames() : 0;
}

int TimeStretchDeckAdapter::seekLengthFrames() const noexcept {
    return ready ? processor.seekLengthFrames() : 0;
}

} // namespace broke
