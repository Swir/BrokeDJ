// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <cstdint>

namespace broke {

struct AudioDeviceRecoveryEvent {
    bool pausePlayback = false;
    bool deviceLost = false;
    bool deviceRecovered = false;
    bool deviceChanged = false;
};

// Observe device usability without coupling the recovery policy to JUCE. The
// concrete app passes JUCE's AudioIODevice pointer, while the constexpr contract
// below proves that a retained-but-closed object is treated as unavailable.
template <typename Device>
[[nodiscard]] constexpr bool audioDeviceIsOpen(Device* device) {
    return device != nullptr && device->isOpen();
}

// Pointer identity is intentionally a runtime-only token. A different live JUCE
// AudioIODevice object is treated conservatively as a device replacement even
// when both the old and new objects are open between message-thread polls.
template <typename Device>
[[nodiscard]] std::uintptr_t audioDeviceIdentity(Device* device) noexcept {
    return reinterpret_cast<std::uintptr_t>(device);
}

// Message-thread policy for the native device lifecycle. It intentionally has
// no JUCE dependency so transition semantics can be compile-time verified and
// reused without moving device I/O or recovery work into the audio callback.
// A non-zero identity token lets the app fail safe on A->B device replacement
// even when no intermediate unavailable observation is visible to the timer.
class AudioDeviceRecoveryPolicy final {
public:
    constexpr void reset(bool deviceAvailable, std::uintptr_t identity = 0) noexcept {
        initialized = true;
        available = deviceAvailable;
        deviceIdentity = deviceAvailable ? identity : 0;
        lossLatched = false;
    }

    [[nodiscard]] constexpr AudioDeviceRecoveryEvent update(
        bool deviceAvailable, std::uintptr_t identity = 0) noexcept {
        if (!initialized) {
            reset(deviceAvailable, identity);
            return {};
        }

        if (deviceAvailable == available) {
            if (!deviceAvailable) return {};

            // Only compare identities when both sides are known. This keeps the
            // old boolean-only call contract neutral while allowing the native
            // app to distinguish two simultaneously valid device objects.
            if (deviceIdentity != 0 && identity != 0 && deviceIdentity != identity) {
                deviceIdentity = identity;
                lossLatched = false;
                return {.pausePlayback = true,
                        .deviceLost = false,
                        .deviceRecovered = false,
                        .deviceChanged = true};
            }
            if (deviceIdentity == 0 && identity != 0) deviceIdentity = identity;
            return {};
        }

        available = deviceAvailable;
        if (!deviceAvailable) {
            deviceIdentity = 0;
            lossLatched = true;
            return {.pausePlayback = true,
                    .deviceLost = true,
                    .deviceRecovered = false,
                    .deviceChanged = false};
        }

        deviceIdentity = identity;
        const bool recovered = lossLatched;
        lossLatched = false;
        return {.pausePlayback = false,
                .deviceLost = false,
                .deviceRecovered = recovered,
                .deviceChanged = false};
    }

    [[nodiscard]] constexpr bool lossIsLatched() const noexcept { return lossLatched; }
    [[nodiscard]] constexpr std::uintptr_t currentDeviceIdentity() const noexcept {
        return deviceIdentity;
    }

private:
    bool initialized = false;
    bool available = false;
    bool lossLatched = false;
    std::uintptr_t deviceIdentity = 0;
};

namespace audio_device_recovery_contract {
struct FakeDevice {
    bool open = false;
    [[nodiscard]] constexpr bool isOpen() { return open; }
};

constexpr bool openStateUsesObjectStateNotPointerPresence() {
    FakeDevice open{true};
    FakeDevice closed{false};
    return !audioDeviceIsOpen<FakeDevice>(nullptr)
        && audioDeviceIsOpen(&open)
        && !audioDeviceIsOpen(&closed);
}

constexpr bool firstObservationIsNeutral() {
    AudioDeviceRecoveryPolicy policy;
    const auto unavailable = policy.update(false, 0);
    if (unavailable.pausePlayback || unavailable.deviceLost || unavailable.deviceRecovered
        || unavailable.deviceChanged)
        return false;
    policy.reset(true, 101);
    const auto available = policy.update(true, 101);
    return !available.pausePlayback && !available.deviceLost && !available.deviceRecovered
        && !available.deviceChanged;
}

constexpr bool lossPausesExactlyOnce() {
    AudioDeviceRecoveryPolicy policy;
    policy.reset(true, 101);
    const auto lost = policy.update(false, 0);
    const auto repeated = policy.update(false, 0);
    return lost.pausePlayback && lost.deviceLost && !lost.deviceRecovered && !lost.deviceChanged
        && policy.lossIsLatched()
        && !repeated.pausePlayback && !repeated.deviceLost && !repeated.deviceRecovered
        && !repeated.deviceChanged;
}

constexpr bool recoveryDoesNotAutoResume() {
    AudioDeviceRecoveryPolicy policy;
    policy.reset(true, 101);
    static_cast<void>(policy.update(false, 0));
    const auto recovered = policy.update(true, 202);
    const auto repeated = policy.update(true, 202);
    return !recovered.pausePlayback && !recovered.deviceLost && recovered.deviceRecovered
        && !recovered.deviceChanged && !policy.lossIsLatched()
        && policy.currentDeviceIdentity() == 202
        && !repeated.pausePlayback && !repeated.deviceLost && !repeated.deviceRecovered
        && !repeated.deviceChanged;
}

constexpr bool initialUnavailableIsNotARecovery() {
    AudioDeviceRecoveryPolicy policy;
    policy.reset(false, 0);
    const auto becameAvailable = policy.update(true, 101);
    return !becameAvailable.pausePlayback && !becameAvailable.deviceLost
        && !becameAvailable.deviceRecovered && !becameAvailable.deviceChanged
        && policy.currentDeviceIdentity() == 101;
}

constexpr bool repeatedLossCyclesRemainFailSafe() {
    AudioDeviceRecoveryPolicy policy;
    policy.reset(true, 101);
    const auto firstLoss = policy.update(false, 0);
    const auto firstRecovery = policy.update(true, 101);
    const auto secondLoss = policy.update(false, 0);
    return firstLoss.pausePlayback && firstRecovery.deviceRecovered
        && secondLoss.pausePlayback && secondLoss.deviceLost && !secondLoss.deviceRecovered
        && !secondLoss.deviceChanged;
}

constexpr bool liveDeviceReplacementPausesExactlyOnce() {
    AudioDeviceRecoveryPolicy policy;
    policy.reset(true, 101);
    const auto replacement = policy.update(true, 202);
    const auto repeated = policy.update(true, 202);
    return replacement.pausePlayback && !replacement.deviceLost
        && !replacement.deviceRecovered && replacement.deviceChanged
        && policy.currentDeviceIdentity() == 202
        && !repeated.pausePlayback && !repeated.deviceLost && !repeated.deviceRecovered
        && !repeated.deviceChanged;
}

constexpr bool unknownIdentityDoesNotCreateFalseReplacement() {
    AudioDeviceRecoveryPolicy policy;
    policy.reset(true, 0);
    const auto learned = policy.update(true, 101);
    const auto unknown = policy.update(true, 0);
    return !learned.pausePlayback && !learned.deviceChanged
        && policy.currentDeviceIdentity() == 101
        && !unknown.pausePlayback && !unknown.deviceChanged
        && policy.currentDeviceIdentity() == 101;
}
} // namespace audio_device_recovery_contract

static_assert(audio_device_recovery_contract::openStateUsesObjectStateNotPointerPresence());
static_assert(audio_device_recovery_contract::firstObservationIsNeutral());
static_assert(audio_device_recovery_contract::lossPausesExactlyOnce());
static_assert(audio_device_recovery_contract::recoveryDoesNotAutoResume());
static_assert(audio_device_recovery_contract::initialUnavailableIsNotARecovery());
static_assert(audio_device_recovery_contract::repeatedLossCyclesRemainFailSafe());
static_assert(audio_device_recovery_contract::liveDeviceReplacementPausesExactlyOnce());
static_assert(audio_device_recovery_contract::unknownIdentityDoesNotCreateFalseReplacement());

} // namespace broke
