// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

namespace broke {

struct AudioDeviceRecoveryEvent {
    bool pausePlayback = false;
    bool deviceLost = false;
    bool deviceRecovered = false;
};

// Message-thread policy for the native device lifecycle. It intentionally has
// no JUCE dependency so transition semantics can be compile-time verified and
// reused without moving device I/O or recovery work into the audio callback.
class AudioDeviceRecoveryPolicy final {
public:
    constexpr void reset(bool deviceAvailable) noexcept {
        initialized = true;
        available = deviceAvailable;
        lossLatched = false;
    }

    [[nodiscard]] constexpr AudioDeviceRecoveryEvent update(bool deviceAvailable) noexcept {
        if (!initialized) {
            reset(deviceAvailable);
            return {};
        }
        if (deviceAvailable == available) return {};

        available = deviceAvailable;
        if (!deviceAvailable) {
            lossLatched = true;
            return {.pausePlayback = true, .deviceLost = true, .deviceRecovered = false};
        }

        const bool recovered = lossLatched;
        lossLatched = false;
        return {.pausePlayback = false, .deviceLost = false, .deviceRecovered = recovered};
    }

    [[nodiscard]] constexpr bool lossIsLatched() const noexcept { return lossLatched; }

private:
    bool initialized = false;
    bool available = false;
    bool lossLatched = false;
};

namespace audio_device_recovery_contract {
constexpr bool firstObservationIsNeutral() {
    AudioDeviceRecoveryPolicy policy;
    const auto unavailable = policy.update(false);
    if (unavailable.pausePlayback || unavailable.deviceLost || unavailable.deviceRecovered)
        return false;
    policy.reset(true);
    const auto available = policy.update(true);
    return !available.pausePlayback && !available.deviceLost && !available.deviceRecovered;
}

constexpr bool lossPausesExactlyOnce() {
    AudioDeviceRecoveryPolicy policy;
    policy.reset(true);
    const auto lost = policy.update(false);
    const auto repeated = policy.update(false);
    return lost.pausePlayback && lost.deviceLost && !lost.deviceRecovered
        && policy.lossIsLatched()
        && !repeated.pausePlayback && !repeated.deviceLost && !repeated.deviceRecovered;
}

constexpr bool recoveryDoesNotAutoResume() {
    AudioDeviceRecoveryPolicy policy;
    policy.reset(true);
    static_cast<void>(policy.update(false));
    const auto recovered = policy.update(true);
    const auto repeated = policy.update(true);
    return !recovered.pausePlayback && !recovered.deviceLost && recovered.deviceRecovered
        && !policy.lossIsLatched()
        && !repeated.pausePlayback && !repeated.deviceLost && !repeated.deviceRecovered;
}

constexpr bool initialUnavailableIsNotARecovery() {
    AudioDeviceRecoveryPolicy policy;
    policy.reset(false);
    const auto becameAvailable = policy.update(true);
    return !becameAvailable.pausePlayback && !becameAvailable.deviceLost
        && !becameAvailable.deviceRecovered;
}

constexpr bool repeatedLossCyclesRemainFailSafe() {
    AudioDeviceRecoveryPolicy policy;
    policy.reset(true);
    const auto firstLoss = policy.update(false);
    const auto firstRecovery = policy.update(true);
    const auto secondLoss = policy.update(false);
    return firstLoss.pausePlayback && firstRecovery.deviceRecovered
        && secondLoss.pausePlayback && secondLoss.deviceLost && !secondLoss.deviceRecovered;
}
} // namespace audio_device_recovery_contract

static_assert(audio_device_recovery_contract::firstObservationIsNeutral());
static_assert(audio_device_recovery_contract::lossPausesExactlyOnce());
static_assert(audio_device_recovery_contract::recoveryDoesNotAutoResume());
static_assert(audio_device_recovery_contract::initialUnavailableIsNotARecovery());
static_assert(audio_device_recovery_contract::repeatedLossCyclesRemainFailSafe());

} // namespace broke
