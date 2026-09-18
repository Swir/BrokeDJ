// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#include "core/TimeStretchSourceBridge.h"

#include <algorithm>
#include <cmath>

namespace broke {
namespace {
constexpr double minimumSampleRate = 8000.0;
constexpr double maximumSampleRate = 384000.0;

float finiteSample(float value) noexcept {
    return std::isfinite(value) ? value : 0.0f;
}
} // namespace

bool TimeStretchSourceBridge::prepare(double newSourceSampleRate, int maxOutputFrames,
                                      double maxPlaybackRate, bool splitComputation) {
    ready = false;
    historyPrimed = false;
    entryFadeRemaining = 0;
    streamStarving = false;
    if (!std::isfinite(newSourceSampleRate)
        || newSourceSampleRate < minimumSampleRate
        || newSourceSampleRate > maximumSampleRate) {
        return false;
    }
    if (!deck.prepare(newSourceSampleRate, maxOutputFrames,
                      maxPlaybackRate, splitComputation)) {
        return false;
    }
    const int scratchFrames = deck.maxInputFrames();
    if (scratchFrames <= 0 || deck.seekLengthFrames() <= 0
        || deck.seekLengthFrames() > scratchFrames) {
        deck.reset();
        return false;
    }

    inputLeft.assign(static_cast<std::size_t>(scratchFrames), 0.0f);
    inputRight.assign(static_cast<std::size_t>(scratchFrames), 0.0f);
    sourceRate = newSourceSampleRate;
    entryFadeFrames = std::max(1, static_cast<int>(std::lround(sourceRate * 0.005)));
    ready = true;
    return true;
}

void TimeStretchSourceBridge::reset() noexcept {
    deck.reset();
    entryFadeRemaining = 0;
    historyPrimed = false;
    streamStarving = false;
}

bool TimeStretchSourceBridge::setPlaybackRate(double playbackRate) noexcept {
    return ready && deck.setPlaybackRate(playbackRate);
}

bool TimeStretchSourceBridge::setPitchSemitones(float semitones) noexcept {
    return ready && deck.setPitchSemitones(semitones);
}

bool TimeStretchSourceBridge::compatible(const Clip& clip) const noexcept {
    if (!ready || !clip.valid() || !std::isfinite(clip.sampleRate)) return false;
    const double tolerance = std::max(1.0e-6, sourceRate * 1.0e-9);
    return std::abs(clip.sampleRate - sourceRate) <= tolerance;
}

bool TimeStretchSourceBridge::gather(const Clip& clip, double startCursor,
                                     bool loop, int frames, bool zeroPadOutside,
                                     std::int64_t& firstMissFrame) noexcept {
    firstMissFrame = -1;
    const auto totalFrames = clip.frames();
    if (!compatible(clip) || !std::isfinite(startCursor)
        || frames <= 0 || frames > deck.maxInputFrames()
        || static_cast<std::size_t>(frames) > inputLeft.size()
        || static_cast<std::size_t>(frames) > inputRight.size()
        || totalFrames <= 0) {
        return false;
    }

    const auto readFrame = [&](std::int64_t frame, int channel, float& value) noexcept {
        if (loop) {
            frame %= totalFrames;
            if (frame < 0) frame += totalFrames;
        } else if (frame < 0 || frame >= totalFrames) {
            if (!zeroPadOutside) return false;
            value = 0.0f;
            return true;
        }

        if (clip.stream) {
            if (!clip.stream->trySample(channel, frame, value)) {
                if (firstMissFrame < 0) firstMissFrame = frame;
                return false;
            }
            value = finiteSample(value);
            return true;
        }

        const auto& data = channel == 0 ? clip.left : clip.right;
        value = finiteSample(data[static_cast<std::size_t>(frame)]);
        return true;
    };

    for (int offset = 0; offset < frames; ++offset) {
        const double position = startCursor + static_cast<double>(offset);
        const auto base = static_cast<std::int64_t>(std::floor(position));
        const float fraction = static_cast<float>(position - static_cast<double>(base));

        float left0 = 0.0f;
        float right0 = 0.0f;
        if (!readFrame(base, 0, left0) || !readFrame(base, 1, right0)) return false;

        float left = left0;
        float right = right0;
        if (fraction > 1.0e-7f) {
            float left1 = 0.0f;
            float right1 = 0.0f;
            if (!readFrame(base + 1, 0, left1) || !readFrame(base + 1, 1, right1))
                return false;
            left = left0 + (left1 - left0) * fraction;
            right = right0 + (right1 - right0) * fraction;
        }
        inputLeft[static_cast<std::size_t>(offset)] = finiteSample(left);
        inputRight[static_cast<std::size_t>(offset)] = finiteSample(right);
    }
    return true;
}

void TimeStretchSourceBridge::beginEntryFade() noexcept {
    entryFadeRemaining = entryFadeFrames;
}

bool TimeStretchSourceBridge::prime(const Clip& clip, double cursor, bool loop) noexcept {
    historyPrimed = false;
    entryFadeRemaining = 0;
    if (!compatible(clip) || !std::isfinite(cursor)) return false;

    const auto totalFrames = clip.frames();
    if (cursor < 0.0 || (!loop && cursor >= static_cast<double>(totalFrames))) return false;

    const int historyFrames = deck.seekLengthFrames();
    if (historyFrames <= 0 || historyFrames > deck.maxInputFrames()) return false;

    const double historyStart = cursor - static_cast<double>(historyFrames);
    std::int64_t missFrame = -1;
    if (!gather(clip, historyStart, loop, historyFrames, true, missFrame)) {
        if (clip.stream && missFrame >= 0) {
            clip.stream->noteStarvation(missFrame);
            streamStarving = true;
        }
        return false;
    }

    if (!deck.primeAfterDiscontinuity(inputLeft.data(), inputRight.data(), historyFrames)) {
        deck.reset();
        return false;
    }
    if (clip.stream && streamStarving) {
        clip.stream->noteRefill();
        streamStarving = false;
    }
    historyPrimed = true;
    beginEntryFade();
    return true;
}

bool TimeStretchSourceBridge::render(const Clip& clip, double cursor, bool loop,
                                     float* outputLeft, float* outputRight,
                                     int outputFrames, double& nextCursor) noexcept {
    if (!historyPrimed || outputLeft == nullptr || outputRight == nullptr
        || !compatible(clip) || !std::isfinite(cursor)) {
        return false;
    }
    const int needed = deck.inputFramesForOutput(outputFrames);
    if (needed <= 0) return false;

    std::int64_t missFrame = -1;
    if (!gather(clip, cursor, loop, needed, false, missFrame)) {
        if (clip.stream && missFrame >= 0) {
            clip.stream->noteStarvation(missFrame);
            streamStarving = true;
        }
        return false;
    }

    if (!deck.processStereo(inputLeft.data(), inputRight.data(), needed,
                            outputLeft, outputRight, outputFrames)) {
        historyPrimed = false;
        entryFadeRemaining = 0;
        return false;
    }

    if (clip.stream && streamStarving) {
        clip.stream->noteRefill();
        streamStarving = false;
        beginEntryFade();
    }

    if (entryFadeRemaining > 0) {
        for (int frame = 0; frame < outputFrames && entryFadeRemaining > 0; ++frame) {
            const float gain = 1.0f
                - static_cast<float>(entryFadeRemaining)
                    / static_cast<float>(entryFadeFrames);
            outputLeft[frame] = finiteSample(outputLeft[frame]) * gain;
            outputRight[frame] = finiteSample(outputRight[frame]) * gain;
            --entryFadeRemaining;
        }
    }

    double advanced = cursor + static_cast<double>(needed);
    const double length = static_cast<double>(clip.frames());
    if (loop) {
        advanced = std::fmod(advanced, length);
        if (advanced < 0.0) advanced += length;
    } else if (advanced > length) {
        historyPrimed = false;
        return false;
    }
    nextCursor = advanced;
    return true;
}

} // namespace broke
