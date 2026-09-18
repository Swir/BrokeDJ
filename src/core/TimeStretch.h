// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <memory>

namespace broke {

// Optional M2 research path for production time-stretch/key-lock. This class is
// deliberately separate from Engine so ordinary playback has no dependency on
// the prototype. prepare() owns all setup/allocation and must run off the audio
// thread. process() rejects blocks above the prepared bounds.
class TimeStretchPrototype final {
public:
    TimeStretchPrototype();
    ~TimeStretchPrototype();
    TimeStretchPrototype(TimeStretchPrototype&&) noexcept;
    TimeStretchPrototype& operator=(TimeStretchPrototype&&) noexcept;
    TimeStretchPrototype(const TimeStretchPrototype&) = delete;
    TimeStretchPrototype& operator=(const TimeStretchPrototype&) = delete;

    [[nodiscard]] bool prepare(double sampleRate, int maxInputFrames, int maxOutputFrames,
                               bool splitComputation = true);
    void reset();

    [[nodiscard]] bool setPitchSemitones(float semitones) noexcept;
    [[nodiscard]] bool processStereo(const float* left, const float* right, int inputFrames,
                                     float* outputLeft, float* outputRight, int outputFrames) noexcept;
    [[nodiscard]] bool primeAfterSeek(const float* left, const float* right, int inputFrames,
                                      double playbackRate) noexcept;

    [[nodiscard]] bool prepared() const noexcept;
    [[nodiscard]] int inputLatencyFrames() const noexcept;
    [[nodiscard]] int outputLatencyFrames() const noexcept;
    [[nodiscard]] int seekLengthFrames() const noexcept;
    [[nodiscard]] int maxInputFrames() const noexcept;
    [[nodiscard]] int maxOutputFrames() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace broke
