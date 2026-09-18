// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#include "core/TimeStretch.h"

#include <signalsmith-stretch/signalsmith-stretch.h>

#include <array>
#include <cmath>
#include <utility>

namespace broke {
namespace {
constexpr int stereoChannels = 2;
constexpr float minPitchSemitones = -24.0f;
constexpr float maxPitchSemitones = 24.0f;
constexpr double minPlaybackRate = 0.25;
constexpr double maxPlaybackRate = 4.0;

struct InputView final {
    std::array<const float*, stereoChannels> channels{};
    [[nodiscard]] const float* operator[](int channel) const noexcept {
        return channels[static_cast<std::size_t>(channel)];
    }
};

struct OutputView final {
    std::array<float*, stereoChannels> channels{};
    [[nodiscard]] float* operator[](int channel) const noexcept {
        return channels[static_cast<std::size_t>(channel)];
    }
};
} // namespace

struct TimeStretchPrototype::Impl final {
    // A fixed seed keeps deterministic fixtures reproducible. The seed is not
    // musical state and this prototype is not yet wired into Engine playback.
    signalsmith::stretch::SignalsmithStretch<float> stretch{0x42524f4b};
    double sampleRate = 0.0;
    int maxInput = 0;
    int maxOutput = 0;
    bool ready = false;
};

TimeStretchPrototype::TimeStretchPrototype() = default;
TimeStretchPrototype::~TimeStretchPrototype() = default;
TimeStretchPrototype::TimeStretchPrototype(TimeStretchPrototype&&) noexcept = default;
TimeStretchPrototype& TimeStretchPrototype::operator=(TimeStretchPrototype&&) noexcept = default;

bool TimeStretchPrototype::prepare(double newSampleRate, int newMaxInputFrames,
                                   int newMaxOutputFrames, bool splitComputation) {
    if (!std::isfinite(newSampleRate) || newSampleRate < 8000.0 || newSampleRate > 384000.0
        || newMaxInputFrames <= 0 || newMaxOutputFrames <= 0
        || newMaxInputFrames > 65536 || newMaxOutputFrames > 65536) {
        return false;
    }

    auto next = std::make_unique<Impl>();
    next->stretch.presetDefault(stereoChannels, static_cast<float>(newSampleRate), splitComputation);
    next->stretch.setTransposeSemitones(0.0f);
    next->sampleRate = newSampleRate;
    next->maxInput = newMaxInputFrames;
    next->maxOutput = newMaxOutputFrames;
    next->ready = true;
    impl = std::move(next);
    return true;
}

void TimeStretchPrototype::reset() {
    if (impl != nullptr && impl->ready) impl->stretch.reset();
}

bool TimeStretchPrototype::setPitchSemitones(float semitones) noexcept {
    if (impl == nullptr || !impl->ready || !std::isfinite(semitones)
        || semitones < minPitchSemitones || semitones > maxPitchSemitones) {
        return false;
    }
    impl->stretch.setTransposeSemitones(semitones);
    return true;
}

bool TimeStretchPrototype::processStereo(const float* left, const float* right, int inputFrames,
                                         float* outputLeft, float* outputRight,
                                         int outputFrames) noexcept {
    if (impl == nullptr || !impl->ready || left == nullptr || right == nullptr
        || outputLeft == nullptr || outputRight == nullptr || inputFrames <= 0 || outputFrames <= 0
        || inputFrames > impl->maxInput || outputFrames > impl->maxOutput) {
        return false;
    }

    InputView input{{left, right}};
    OutputView output{{outputLeft, outputRight}};
    impl->stretch.process(input, inputFrames, output, outputFrames);
    return true;
}

bool TimeStretchPrototype::primeAfterSeek(const float* left, const float* right, int inputFrames,
                                          double playbackRate) noexcept {
    if (impl == nullptr || !impl->ready || left == nullptr || right == nullptr || inputFrames <= 0
        || inputFrames > impl->maxInput || !std::isfinite(playbackRate)
        || playbackRate < minPlaybackRate || playbackRate > maxPlaybackRate) {
        return false;
    }

    InputView input{{left, right}};
    impl->stretch.seek(input, inputFrames, playbackRate);
    return true;
}

bool TimeStretchPrototype::prepared() const noexcept {
    return impl != nullptr && impl->ready;
}

int TimeStretchPrototype::inputLatencyFrames() const noexcept {
    return prepared() ? impl->stretch.inputLatency() : 0;
}

int TimeStretchPrototype::outputLatencyFrames() const noexcept {
    return prepared() ? impl->stretch.outputLatency() : 0;
}

int TimeStretchPrototype::seekLengthFrames() const noexcept {
    return prepared() ? impl->stretch.seekLength() : 0;
}

int TimeStretchPrototype::maxInputFrames() const noexcept {
    return prepared() ? impl->maxInput : 0;
}

int TimeStretchPrototype::maxOutputFrames() const noexcept {
    return prepared() ? impl->maxOutput : 0;
}

} // namespace broke
