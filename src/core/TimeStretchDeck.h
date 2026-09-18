// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "core/TimeStretch.h"

#include <cstdint>

namespace broke {

// Bounded clock/buffer contract for the optional M2 key-lock research path.
//
// This adapter deliberately does not own disk/cache readers and is not wired to
// Engine playback yet. The caller asks how many contiguous source frames are
// required for the next fixed-size output block, provides exactly that many
// frames, and advances the source clock only after successful processing.
// Failed blocks leave the clock unchanged so the production pitch-changing
// rate converter can be used as an immediate fallback by a future integration.
class TimeStretchDeckAdapter final {
public:
    [[nodiscard]] bool prepare(double sampleRate, int maxOutputFrames,
                               double maxPlaybackRate = 4.0,
                               bool splitComputation = true);
    void reset() noexcept;

    [[nodiscard]] bool setPlaybackRate(double playbackRate) noexcept;
    [[nodiscard]] bool setPitchSemitones(float semitones) noexcept;

    [[nodiscard]] bool prepared() const noexcept { return ready; }
    [[nodiscard]] double playbackRate() const noexcept { return rate; }
    [[nodiscard]] int maxOutputFrames() const noexcept { return maxOutput; }
    [[nodiscard]] int maxInputFrames() const noexcept { return maxInput; }

    // Returns 0 if the request cannot be represented within the prepared
    // bounds. For valid realtime block sizes the returned count is at least 1.
    [[nodiscard]] int inputFramesForOutput(int outputFrames) const noexcept;

    // inputFrames must equal inputFramesForOutput(outputFrames). On failure the
    // source clock/carry are unchanged and output buffers should be ignored.
    [[nodiscard]] bool processStereo(const float* left, const float* right, int inputFrames,
                                     float* outputLeft, float* outputRight,
                                     int outputFrames) noexcept;

    // Seek/load/loop discontinuities reset fractional source-clock carry and
    // the processor state. Priming is expected off the audio callback until a
    // measured realtime contract explicitly proves otherwise.
    [[nodiscard]] bool primeAfterDiscontinuity(const float* left, const float* right,
                                               int inputFrames) noexcept;

    [[nodiscard]] std::int64_t sourceFramesConsumed() const noexcept { return consumed; }
    [[nodiscard]] double fractionalSourceCarry() const noexcept { return carry; }
    [[nodiscard]] int inputLatencyFrames() const noexcept;
    [[nodiscard]] int outputLatencyFrames() const noexcept;
    [[nodiscard]] int seekLengthFrames() const noexcept;

private:
    [[nodiscard]] bool validOutputRequest(int outputFrames) const noexcept;

    TimeStretchPrototype processor;
    double sampleRate = 0.0;
    double rate = 1.0;
    double maxRate = 4.0;
    double carry = 0.0;
    std::int64_t consumed = 0;
    int maxInput = 0;
    int maxOutput = 0;
    bool ready = false;
};

} // namespace broke
