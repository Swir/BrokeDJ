// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "core/Engine.h"
#include "core/EngineKeyLockDeckOwner.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace broke {

// Message-thread/native-app ownership for the optional key-lock renderer.
//
// This class deliberately contains no JUCE types so the lifecycle contract can
// be qualified in deterministic core tests. The caller must obey the two hard
// boundaries below:
//   * configureAudioStopped()/releaseAudioStopped() only while callbacks are stopped;
//   * all other mutating calls from one serialized non-audio owner thread.
//
// Live discontinuities fail closed immediately: mark/seek/rate/loop/pitch
// changes disarm the owner so Engine uses its built-in production converter.
// Restaging is deferred while the deck is playing; a paused deck can be staged
// deterministically before play resumes. The lifecycle also compares the
// authoritative loop/rate values passed to service() with the last staged
// snapshot. This catches controller/device paths that change Engine controls
// without first notifying the UI adapter, preventing a stale key-lock snapshot
// from remaining armed indefinitely after Engine has already rejected it.
// Invalid pitch requests are latched fail-closed until a later valid pitch
// request explicitly clears the validation barrier; the previous valid pitch
// value is never silently re-armed by the timer after an invalid control write.
// This intentionally favors reliable audio over seamless live key-lock
// automation until that harder handoff is separately qualified.
class KeyLockDeckLifecycle final {
public:
    enum class ServiceStatus {
        idle,
        staged,
        disabled,
        noClip,
        notConfigured,
        deferredWhilePlaying,
        busy,
        invalidControl,
        prepareFailed,
        stageFailed
    };

    explicit KeyLockDeckLifecycle(Engine& target) noexcept : engine(target) {}
    KeyLockDeckLifecycle(const KeyLockDeckLifecycle&) = delete;
    KeyLockDeckLifecycle& operator=(const KeyLockDeckLifecycle&) = delete;

    [[nodiscard]] bool configureAudioStopped(double deviceSampleRate,
                                             int maxDeviceFrames) noexcept {
        releaseAudioStopped();
        if (!std::isfinite(deviceSampleRate) || deviceSampleRate < 8000.0
            || deviceSampleRate > 192000.0 || maxDeviceFrames <= 0
            || maxDeviceFrames > defaultMaxAudioBlockFrames) {
            return false;
        }

        for (auto& owner : owners) {
            if (!owner.configureDevice(deviceSampleRate, maxDeviceFrames)) {
                releaseAudioStopped();
                return false;
            }
        }
        for (std::size_t deck = 0; deck < deckCount; ++deck) {
            if (!engine.setDeckSourceRenderer(deck, &owners[deck])) {
                releaseAudioStopped();
                return false;
            }
            states[deck].dirty = states[deck].enabled && states[deck].clip != nullptr;
            states[deck].hasStagedTransport = false;
        }
        configuredFlag = true;
        return true;
    }

    void releaseAudioStopped() noexcept {
        for (std::size_t deck = 0; deck < deckCount; ++deck) {
            static_cast<void>(engine.setDeckSourceRenderer(deck, nullptr));
            owners[deck].resetWhenAudioStopped();
            states[deck].dirty = states[deck].enabled && states[deck].clip != nullptr;
            states[deck].hasStagedTransport = false;
        }
        configuredFlag = false;
    }

    [[nodiscard]] bool setEnabled(std::size_t deck, bool enabled) noexcept {
        if (deck >= deckCount) return false;
        auto& state = states[deck];
        if (state.enabled == enabled) return true;
        state.enabled = enabled;
        state.dirty = enabled && state.clip != nullptr;
        state.hasExplicitCursor = false;
        state.hasStagedTransport = false;
        owners[deck].disarm();
        return true;
    }

    [[nodiscard]] bool setPitchSemitones(std::size_t deck, float semitones) noexcept {
        if (deck >= deckCount) return false;
        auto& state = states[deck];
        if (!std::isfinite(semitones) || semitones < -24.0f || semitones > 24.0f) {
            state.pitchValid = false;
            owners[deck].disarm();
            state.dirty = state.enabled && state.clip != nullptr;
            state.hasStagedTransport = false;
            return false;
        }

        // A valid request is also the explicit recovery action after an invalid
        // request. Do this before the equality fast-path so re-applying the last
        // valid pitch can clear the fail-closed latch without manufacturing a
        // different pitch value merely to force restaging.
        state.pitchValid = true;
        if (std::abs(state.pitchSemitones - semitones) <= 1.0e-6f) return true;
        state.pitchSemitones = semitones;
        owners[deck].disarm();
        state.dirty = state.enabled && state.clip != nullptr;
        state.hasExplicitCursor = false;
        state.hasStagedTransport = false;
        return true;
    }

    // Call only after Engine::submit() has accepted the same immutable object.
    // Moving the unique_ptr into Engine does not change the Clip object's address.
    void noteClipSubmitted(std::size_t deck, const Clip* clip) noexcept {
        if (deck >= deckCount) return;
        auto& state = states[deck];
        owners[deck].disarm();
        state.clip = clip;
        state.dirty = state.enabled && clip != nullptr;
        state.hasStagedTransport = false;
        state.hasExplicitCursor = clip != nullptr;
        state.explicitCursorFrames = 0.0;
    }

    // Loop/rate changes and other discontinuities intentionally disarm first;
    // ordinary Engine playback remains the immediate audible fallback.
    void noteTransportControlChanged(std::size_t deck) noexcept {
        if (deck >= deckCount) return;
        owners[deck].disarm();
        auto& state = states[deck];
        state.dirty = state.enabled && state.clip != nullptr;
        state.hasExplicitCursor = false;
        state.hasStagedTransport = false;
    }

    void noteSeekNormalized(std::size_t deck, double normalized) noexcept {
        if (deck >= deckCount) return;
        owners[deck].disarm();
        auto& state = states[deck];
        state.dirty = state.enabled && state.clip != nullptr;
        state.hasExplicitCursor = false;
        state.hasStagedTransport = false;
        if (!state.clip || !state.clip->valid() || !std::isfinite(normalized)) return;
        const auto frameCount = state.clip->frames();
        if (frameCount <= 0) return;
        const double clamped = std::clamp(normalized, 0.0, 1.0);
        state.explicitCursorFrames = clamped * static_cast<double>(frameCount - 1);
        state.hasExplicitCursor = true;
    }

    [[nodiscard]] ServiceStatus service(std::size_t deck, double positionSeconds,
                                        bool loop, double playbackRate,
                                        bool playing) {
        if (deck >= deckCount) return ServiceStatus::invalidControl;
        if (!configuredFlag) return ServiceStatus::notConfigured;
        auto& state = states[deck];
        auto& owner = owners[deck];
        if (!state.enabled) {
            owner.disarm();
            state.dirty = false;
            state.hasExplicitCursor = false;
            state.hasStagedTransport = false;
            return ServiceStatus::disabled;
        }
        if (!state.clip || !state.clip->valid()) {
            owner.disarm();
            state.hasStagedTransport = false;
            return ServiceStatus::noClip;
        }
        if (!state.pitchValid) {
            owner.disarm();
            state.dirty = true;
            state.hasStagedTransport = false;
            return ServiceStatus::invalidControl;
        }
        if (!std::isfinite(playbackRate) || playbackRate < 0.5 || playbackRate > 1.5
            || !std::isfinite(positionSeconds) || positionSeconds < 0.0) {
            owner.disarm();
            state.dirty = true;
            state.hasStagedTransport = false;
            return ServiceStatus::invalidControl;
        }

        // Do not rely exclusively on every UI/controller path remembering to
        // call noteTransportControlChanged(). EngineKeyLockSource already rejects
        // a changed loop/rate snapshot in the callback; mirror that boundary here
        // so the non-audio owner marks itself dirty and can recover once paused.
        if (state.hasStagedTransport
            && (state.stagedLoop != loop
                || !samePlaybackRate(state.stagedPlaybackRate, playbackRate))) {
            owner.disarm();
            state.dirty = true;
            state.hasExplicitCursor = false;
            state.hasStagedTransport = false;
        }

        if (!state.dirty) return ServiceStatus::idle;
        if (playing) return ServiceStatus::deferredWhilePlaying;

        const auto frameCount = state.clip->frames();
        if (frameCount <= 0) return ServiceStatus::noClip;
        double cursor = state.hasExplicitCursor
            ? state.explicitCursorFrames
            : positionSeconds * state.clip->sampleRate;
        if (!std::isfinite(cursor)) {
            owner.disarm();
            state.hasStagedTransport = false;
            return ServiceStatus::invalidControl;
        }
        cursor = std::clamp(cursor, 0.0, static_cast<double>(frameCount - 1));

        EngineKeyLockDeckOwner::ControlSnapshot controls;
        controls.playbackRate = playbackRate;
        controls.pitchSemitones = state.pitchSemitones;
        controls.enabled = true;
        const auto result = owner.stage(*state.clip, cursor, loop, controls);
        switch (result) {
            case EngineKeyLockDeckOwner::StageStatus::staged:
                state.dirty = false;
                state.hasExplicitCursor = false;
                state.stagedPlaybackRate = playbackRate;
                state.stagedLoop = loop;
                state.hasStagedTransport = true;
                return ServiceStatus::staged;
            case EngineKeyLockDeckOwner::StageStatus::busy:
                return ServiceStatus::busy;
            case EngineKeyLockDeckOwner::StageStatus::prepareFailed:
                return ServiceStatus::prepareFailed;
            case EngineKeyLockDeckOwner::StageStatus::stageFailed:
                return ServiceStatus::stageFailed;
            case EngineKeyLockDeckOwner::StageStatus::invalidClip:
            case EngineKeyLockDeckOwner::StageStatus::invalidCursor:
                return ServiceStatus::invalidControl;
            case EngineKeyLockDeckOwner::StageStatus::notConfigured:
                return ServiceStatus::notConfigured;
            case EngineKeyLockDeckOwner::StageStatus::disabled:
                return ServiceStatus::disabled;
        }
        return ServiceStatus::stageFailed;
    }

    [[nodiscard]] bool configured() const noexcept { return configuredFlag; }
    [[nodiscard]] bool enabled(std::size_t deck) const noexcept {
        return deck < deckCount && states[deck].enabled;
    }
    [[nodiscard]] bool dirty(std::size_t deck) const noexcept {
        return deck < deckCount && states[deck].dirty;
    }
    [[nodiscard]] bool armed(std::size_t deck) const noexcept {
        return deck < deckCount && owners[deck].armed();
    }
    [[nodiscard]] bool lastRenderAccepted(std::size_t deck) const noexcept {
        return deck < deckCount && owners[deck].lastRenderAccepted();
    }
    [[nodiscard]] std::uint64_t generation(std::size_t deck) const noexcept {
        return deck < deckCount ? owners[deck].generation() : 0;
    }

private:
    [[nodiscard]] static bool samePlaybackRate(double a, double b) noexcept {
        constexpr double absoluteTolerance = 1.0e-7;
        constexpr double relativeTolerance = 1.0e-6;
        const double scale = std::max(1.0, std::max(std::abs(a), std::abs(b)));
        return std::abs(a - b) <= std::max(absoluteTolerance, relativeTolerance * scale);
    }

    struct DeckState final {
        const Clip* clip = nullptr;
        float pitchSemitones = 0.0f;
        double explicitCursorFrames = 0.0;
        double stagedPlaybackRate = 1.0;
        bool enabled = false;
        bool dirty = false;
        bool hasExplicitCursor = false;
        bool stagedLoop = false;
        bool hasStagedTransport = false;
        bool pitchValid = true;
    };

    Engine& engine;
    std::array<EngineKeyLockDeckOwner, deckCount> owners;
    std::array<DeckState, deckCount> states;
    bool configuredFlag = false;
};

} // namespace broke
