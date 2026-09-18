// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "core/EngineKeyLockSource.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace broke {

// Non-realtime owner for EngineKeyLockSource snapshots.
//
// The audio callback sees one stable, fully prepared source at a time. Control
// code prepares/stages the inactive slot and publishes it atomically only after
// success. The previously active slot is kept alive until a later staging pass,
// and a single hazard index prevents that pass from touching a slot still used
// by an in-flight callback. This gives load/seek/loop/rate/pitch owners a safe
// fail-closed handoff without allocating, locking, decoding or re-priming on the
// realtime thread.
//
// Lifecycle:
//   * configureDevice() only while audio is stopped;
//   * stage() from one serialized non-audio owner thread;
//   * install this object once with Engine::setDeckSourceRenderer();
//   * disarm() may run from the owner thread while audio is active;
//   * resetWhenAudioStopped() only after the callback has stopped.
//
// stage() is intentionally non-blocking with respect to the audio callback. If
// the inactive slot is still protected by an in-flight callback it returns
// busy; the owner can retry on its next normal control/timer pass. Engine keeps
// its built-in production rate converter as the immediate audible fallback.
class EngineKeyLockDeckOwner final : public DeckSourceRenderer {
public:
    using ControlSnapshot = EngineKeyLockSource::ControlSnapshot;

    enum class StageStatus : std::uint8_t {
        staged,
        disabled,
        notConfigured,
        invalidClip,
        invalidCursor,
        busy,
        prepareFailed,
        stageFailed
    };

    EngineKeyLockDeckOwner() = default;
    EngineKeyLockDeckOwner(const EngineKeyLockDeckOwner&) = delete;
    EngineKeyLockDeckOwner& operator=(const EngineKeyLockDeckOwner&) = delete;

    // Must not race render(). It does not prime a clip; it only establishes the
    // bounded device-side limits later used by stage().
    [[nodiscard]] bool configureDevice(double newDeviceSampleRate,
                                       int newMaxDeviceFrames,
                                       double newMaxPlaybackRate = 4.0,
                                       bool newSplitComputation = true) noexcept {
        resetWhenAudioStopped();
        if (!std::isfinite(newDeviceSampleRate)
            || newDeviceSampleRate < 8000.0 || newDeviceSampleRate > 192000.0
            || newMaxDeviceFrames <= 0 || newMaxDeviceFrames > defaultMaxAudioBlockFrames
            || !std::isfinite(newMaxPlaybackRate)
            || newMaxPlaybackRate < 0.5 || newMaxPlaybackRate > 8.0) {
            return false;
        }
        deviceSampleRate = newDeviceSampleRate;
        maxDeviceFrames = newMaxDeviceFrames;
        maxPlaybackRate = newMaxPlaybackRate;
        splitComputation = newSplitComputation;
        configuredFlag = true;
        return true;
    }

    // Serialized non-audio operation. A successful call publishes a complete
    // snapshot atomically; a failed call leaves the prior active snapshot (or
    // the ordinary Engine fallback after disarm) untouched.
    [[nodiscard]] StageStatus stage(const Clip& clip, double cursor, bool loop,
                                    const ControlSnapshot& controls) {
        if (!configuredFlag) return StageStatus::notConfigured;
        if (!controls.enabled) {
            disarm();
            return StageStatus::disabled;
        }
        if (!clip.valid()) return StageStatus::invalidClip;
        if (!std::isfinite(cursor) || cursor < 0.0
            || cursor > static_cast<double>(clip.frames())) {
            return StageStatus::invalidCursor;
        }

        const int current = activeSlot.load(std::memory_order_acquire);
        const int target = current == 0 ? 1 : 0;
        if (hazardSlot.load(std::memory_order_acquire) == target)
            return StageStatus::busy;

        auto& candidate = slots[static_cast<std::size_t>(target)];
        bool prepared = false;
        try {
            prepared = candidate.prepare(clip.sampleRate, deviceSampleRate,
                                         maxDeviceFrames, maxPlaybackRate,
                                         splitComputation);
        } catch (...) {
            return StageStatus::prepareFailed;
        }
        if (!prepared) return StageStatus::prepareFailed;

        // A callback that loaded an older active index before the previous
        // publication may publish its hazard after our first check. It cannot
        // use the slot after its active-index validation, but returning busy
        // here keeps the ownership rule conservative and easy to audit.
        if (hazardSlot.load(std::memory_order_acquire) == target)
            return StageStatus::busy;
        if (!candidate.stage(clip, cursor, loop, controls))
            return StageStatus::stageFailed;

        activeSlot.store(target, std::memory_order_release);
        lastAccepted.store(false, std::memory_order_relaxed);
        publishedGeneration.fetch_add(1, std::memory_order_relaxed);
        return StageStatus::staged;
    }

    // Safe fail-closed publication: no source is selected after this store.
    // Existing in-flight readers finish against their retained slot while later
    // callbacks immediately fall back to Engine's built-in converter.
    void disarm() noexcept {
        activeSlot.store(-1, std::memory_order_release);
        lastAccepted.store(false, std::memory_order_relaxed);
        publishedGeneration.fetch_add(1, std::memory_order_relaxed);
    }

    // Audio must already be stopped. This is the only operation that mutates
    // both slots directly.
    void resetWhenAudioStopped() noexcept {
        activeSlot.store(-1, std::memory_order_release);
        hazardSlot.store(-1, std::memory_order_release);
        for (auto& slot : slots) slot.disarm();
        configuredFlag = false;
        deviceSampleRate = 0.0;
        maxDeviceFrames = 0;
        maxPlaybackRate = 4.0;
        splitComputation = true;
        lastAccepted.store(false, std::memory_order_relaxed);
    }

    [[nodiscard]] bool render(const Clip& clip, double cursor, bool loop,
                              double playbackRate,
                              float* outputLeft, float* outputRight,
                              int deviceFrames, double& nextTransportCursor,
                              double& nextAudibleCursor) noexcept override {
        // Bounded retries keep the callback work finite even if a non-audio
        // owner publishes several snapshots around one callback boundary.
        for (int attempt = 0; attempt < 3; ++attempt) {
            const int selected = activeSlot.load(std::memory_order_acquire);
            if (selected < 0 || selected >= static_cast<int>(slots.size())) {
                hazardSlot.store(-1, std::memory_order_release);
                lastAccepted.store(false, std::memory_order_relaxed);
                return false;
            }

            hazardSlot.store(selected, std::memory_order_release);
            if (selected != activeSlot.load(std::memory_order_acquire)) {
                hazardSlot.store(-1, std::memory_order_release);
                continue;
            }

            const bool accepted = slots[static_cast<std::size_t>(selected)].render(
                clip, cursor, loop, playbackRate, outputLeft, outputRight,
                deviceFrames, nextTransportCursor, nextAudibleCursor);
            lastAccepted.store(accepted, std::memory_order_relaxed);
            hazardSlot.store(-1, std::memory_order_release);
            return accepted;
        }

        hazardSlot.store(-1, std::memory_order_release);
        lastAccepted.store(false, std::memory_order_relaxed);
        return false;
    }

    [[nodiscard]] bool configured() const noexcept { return configuredFlag; }
    [[nodiscard]] bool armed() const noexcept {
        return activeSlot.load(std::memory_order_acquire) >= 0;
    }
    [[nodiscard]] bool lastRenderAccepted() const noexcept {
        return lastAccepted.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::uint64_t generation() const noexcept {
        return publishedGeneration.load(std::memory_order_relaxed);
    }

private:
    std::array<EngineKeyLockSource, 2> slots;
    std::atomic<int> activeSlot{-1};
    std::atomic<int> hazardSlot{-1};
    std::atomic<bool> lastAccepted{false};
    std::atomic<std::uint64_t> publishedGeneration{0};
    double deviceSampleRate = 0.0;
    double maxPlaybackRate = 4.0;
    int maxDeviceFrames = 0;
    bool splitComputation = true;
    bool configuredFlag = false;
};

} // namespace broke
