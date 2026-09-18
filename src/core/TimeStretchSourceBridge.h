// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "core/Engine.h"
#include "core/TimeStretchDeck.h"

#include <cstdint>
#include <vector>

namespace broke {

// Opt-in bridge between real BrokeDJ Clip/StreamCache sources and the
// bounded time-stretch deck adapter. This class is research infrastructure:
// production Engine playback does not instantiate it yet.
//
// prepare() owns all scratch allocation. prime() is intended for a
// non-audio/discontinuity preparation point. render() performs bounded
// source reads plus time-stretch processing without resizing buffers,
// blocking, decoding or performing file I/O.
class TimeStretchSourceBridge final {
public:
    [[nodiscard]] bool prepare(double sourceSampleRate, int maxOutputFrames,
                               double maxPlaybackRate = 4.0,
                               bool splitComputation = true);
    void reset() noexcept;

    [[nodiscard]] bool setPlaybackRate(double playbackRate) noexcept;
    [[nodiscard]] bool setPitchSemitones(float semitones) noexcept;

    // Prime processor history immediately before cursor. Out-of-range history
    // before the beginning of a non-looping clip is zero padded. A streamed
    // cache miss fails closed and requests refill through StreamCache.
    [[nodiscard]] bool prime(const Clip& clip, double cursor, bool loop) noexcept;

    // Render one bounded block from cursor. nextCursor is modified only on
    // success. A cache miss, EOF, source-rate mismatch or processor failure
    // returns false so the caller can keep using the production resampler
    // without advancing transport.
    [[nodiscard]] bool render(const Clip& clip, double cursor, bool loop,
                              float* outputLeft, float* outputRight,
                              int outputFrames, double& nextCursor) noexcept;

    [[nodiscard]] bool prepared() const noexcept { return ready; }
    [[nodiscard]] bool primed() const noexcept { return historyPrimed; }
    [[nodiscard]] double sourceSampleRate() const noexcept { return sourceRate; }
    [[nodiscard]] int maxInputFrames() const noexcept { return deck.maxInputFrames(); }
    [[nodiscard]] int maxOutputFrames() const noexcept { return deck.maxOutputFrames(); }
    [[nodiscard]] int inputLatencyFrames() const noexcept { return deck.inputLatencyFrames(); }
    [[nodiscard]] int outputLatencyFrames() const noexcept { return deck.outputLatencyFrames(); }
    [[nodiscard]] int seekLengthFrames() const noexcept { return deck.seekLengthFrames(); }
    [[nodiscard]] std::int64_t sourceFramesConsumed() const noexcept {
        return deck.sourceFramesConsumed();
    }
    [[nodiscard]] double fractionalSourceCarry() const noexcept {
        return deck.fractionalSourceCarry();
    }

private:
    [[nodiscard]] bool compatible(const Clip& clip) const noexcept;
    [[nodiscard]] bool gather(const Clip& clip, double startCursor, bool loop,
                              int frames, bool zeroPadOutside,
                              std::int64_t& firstMissFrame) noexcept;
    void beginEntryFade() noexcept;

    TimeStretchDeckAdapter deck;
    std::vector<float> inputLeft;
    std::vector<float> inputRight;
    double sourceRate = 0.0;
    int entryFadeFrames = 1;
    int entryFadeRemaining = 0;
    bool ready = false;
    bool historyPrimed = false;
    bool streamStarving = false;
};

} // namespace broke
