// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <algorithm>
#include <cmath>

namespace broke {

struct MeterBallisticsSnapshot final {
    float displayDb = -60.0f;
    float holdDb = -60.0f;
    bool overloadLatched = false;
};

// JUCE-independent presentation ballistics for sampled channel meters.
//
// This helper is deliberately outside Engine::process(): the audio callback
// publishes atomic block peaks and the message-thread UI samples them. Attack
// is immediate, release is rate-limited, a finite peak-hold marker decays after
// a bounded hold interval, and overload latches until the operator clears it.
// None of these values are true-peak or loudness measurements.
class MeterBallistics final {
public:
    static constexpr float floorDb = -60.0f;
    static constexpr float ceilingDb = 6.0f;
    static constexpr float defaultSampleSeconds = 1.0f / 25.0f;
    static constexpr float releaseDbPerSecond = 18.0f;
    static constexpr float peakHoldSeconds = 1.20f;
    static constexpr float peakHoldReleaseDbPerSecond = 24.0f;

    void reset() noexcept {
        displayDb = floorDb;
        holdDb = floorDb;
        holdRemainingSeconds = 0.0f;
        overloadLatched = false;
    }

    void pushLinear(float linearPeak, bool sourceOverload, float elapsedSeconds) noexcept {
        const float elapsed = sanitiseElapsed(elapsedSeconds);
        const float measuredDb = linearToDb(linearPeak);

        displayDb = nextDisplayDb(displayDb, measuredDb, elapsed);

        if (measuredDb >= holdDb) {
            holdDb = measuredDb;
            holdRemainingSeconds = peakHoldSeconds;
        } else if (holdRemainingSeconds > 0.0f) {
            holdRemainingSeconds = std::max(0.0f, holdRemainingSeconds - elapsed);
        } else {
            holdDb = std::max(displayDb,
                              holdDb - peakHoldReleaseDbPerSecond * elapsed);
        }

        // The explicit block overload flag remains authoritative. The sampled
        // dB value is also accepted so a visible >= 0 dBFS peak cannot fail to
        // light the latch merely because the UI sampled between adjacent audio
        // blocks.
        overloadLatched = overloadLatched || sourceOverload || measuredDb >= 0.0f;
    }

    void clearOverloadLatch() noexcept { overloadLatched = false; }

    [[nodiscard]] MeterBallisticsSnapshot snapshot() const noexcept {
        return {displayDb, holdDb, overloadLatched};
    }

    [[nodiscard]] static float linearToDb(float linearPeak) noexcept {
        if (!std::isfinite(linearPeak) || linearPeak <= 1.0e-9f) return floorDb;
        const float value = 20.0f * std::log10(linearPeak);
        return std::isfinite(value) ? std::clamp(value, floorDb, ceilingDb) : floorDb;
    }

    [[nodiscard]] static constexpr float normalisedDb(float db) noexcept {
        const float bounded = db < floorDb ? floorDb : (db > ceilingDb ? ceilingDb : db);
        return (bounded - floorDb) / (ceilingDb - floorDb);
    }

    [[nodiscard]] static constexpr float nextDisplayDb(float currentDb,
                                                        float measuredDb,
                                                        float elapsedSeconds) noexcept {
        const float current = currentDb < floorDb ? floorDb
                            : currentDb > ceilingDb ? ceilingDb : currentDb;
        const float measured = measuredDb < floorDb ? floorDb
                             : measuredDb > ceilingDb ? ceilingDb : measuredDb;
        if (measured >= current) return measured;
        const float released = current - releaseDbPerSecond * elapsedSeconds;
        return released < measured ? measured : (released < floorDb ? floorDb : released);
    }

private:
    [[nodiscard]] static float sanitiseElapsed(float elapsedSeconds) noexcept {
        // Large timer stalls must not collapse the meter instantly. A resumed UI
        // simply continues with the nominal sample interval and converges on the
        // following ticks.
        return std::isfinite(elapsedSeconds) && elapsedSeconds > 0.0f && elapsedSeconds <= 0.25f
            ? elapsedSeconds : defaultSampleSeconds;
    }

    float displayDb = floorDb;
    float holdDb = floorDb;
    float holdRemainingSeconds = 0.0f;
    bool overloadLatched = false;
};

// Compile-time regression guards for the dependency-free state math. Runtime
// conversion/sanitisation is additionally exercised whenever the native app is
// built and its existing smoke suite instantiates ChannelPeakMeter.
static_assert(MeterBallistics::normalisedDb(MeterBallistics::floorDb) == 0.0f);
static_assert(MeterBallistics::normalisedDb(MeterBallistics::ceilingDb) == 1.0f);
static_assert(MeterBallistics::nextDisplayDb(-24.0f, -6.0f, 0.04f) == -6.0f);
static_assert(MeterBallistics::nextDisplayDb(-6.0f, -24.0f, 0.04f) < -6.0f);
static_assert(MeterBallistics::nextDisplayDb(-6.0f, -24.0f, 0.04f) > -7.0f);

} // namespace broke
