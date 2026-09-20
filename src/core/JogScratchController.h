// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "Engine.h"
#include "PerformanceDeckOwner.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace broke {

// Message/controller-thread owner for the first bounded jog/scratch workflow.
// It deliberately reuses the production playback-rate + Reverse transport
// instead of introducing a second audio renderer. This gives controllers a
// real audible bidirectional platter gesture while preserving Engine's existing
// smoothing/de-click and realtime callback contract.
//
// Scope is intentionally conservative: the gesture owns ordinary forward /
// reverse transport only. Reviewed Beat Loop, Slip/Slip-Reverse and external
// source renderers remain fail-closed boundaries. The speed envelope matches
// the production Engine rate range (0.5x..1.5x); wider vinyl-style scratching,
// inertia and slip-scratch are future work and must not be inferred here.
class JogScratchController final {
public:
    enum class Result {
        applied,
        invalidDeck,
        invalidRequest,
        trackUnavailable,
        busy,
        notActive
    };

    static constexpr double minAudibleSpeed = 0.5;
    static constexpr double maxAudibleSpeed = 1.5;
    static constexpr double stopDeadzone = 0.04;

    JogScratchController(Engine& targetEngine,
                         PerformanceDeckOwner& targetOwner,
                         std::size_t deckIndex) noexcept
        : engine(targetEngine), owner(targetOwner), deck(deckIndex) {}

    JogScratchController(const JogScratchController&) = delete;
    JogScratchController& operator=(const JogScratchController&) = delete;

    // A controller disappearing while it owns the platter must never leak a
    // transient Reverse/playing state into shutdown, device teardown or the
    // next controller instance. Forced teardown deliberately stops instead of
    // restoring the prior playing flag.
    ~JogScratchController() {
        static_cast<void>(cancel());
    }

    [[nodiscard]] bool active() const noexcept { return gestureActive; }

    // Acquire transient platter ownership. Existing Reverse/Slip or reviewed
    // Beat Loop ownership is rejected instead of being silently destroyed.
    [[nodiscard]] Result begin() noexcept {
        if (deck >= deckCount || !owner.validDeck()) return Result::invalidDeck;
        if (gestureActive) return Result::busy;
        const double duration = engine.meter(deck).duration.load(std::memory_order_acquire);
        if (!std::isfinite(duration) || duration <= 0.0) return Result::trackUnavailable;
        if (conflictingOwnership()
            || owner.reverseSlipMode() != PerformanceDeckOwner::ReverseSlipMode::forward) {
            return Result::busy;
        }

        auto& control = engine.control(deck);
        savedPlaying = control.playing.load(std::memory_order_acquire);
        const float currentRate = control.rate.load(std::memory_order_acquire);
        savedRate = std::isfinite(currentRate)
            ? std::clamp(currentRate, 0.5f, 1.5f)
            : 1.0f;
        gestureActive = true;

        // A touched platter starts stationary until the first signed velocity
        // arrives. No callback-side state or allocation is introduced.
        control.playing.store(false, std::memory_order_release);
        control.rate.store(1.0f, std::memory_order_release);
        return Result::applied;
    }

    // Signed speed: positive = forward, negative = reverse. Magnitude is a
    // source/playback speed ratio and is bounded to Engine's qualified range.
    // Values inside the deadzone hold the platter stopped. Ownership is
    // revalidated on every controller update so a Beat Loop or Slip action that
    // arrives after begin() cannot silently turn an ordinary scratch gesture
    // into a different transport mode.
    [[nodiscard]] Result setVelocity(double signedSpeed) noexcept {
        if (!gestureActive) return Result::notActive;
        if (!std::isfinite(signedSpeed) || std::abs(signedSpeed) > maxAudibleSpeed)
            return Result::invalidRequest;
        if (conflictingOwnership()) return Result::busy;

        auto& control = engine.control(deck);
        if (std::abs(signedSpeed) <= stopDeadzone) {
            const auto reverseResult = owner.setReverseEnabled(false);
            if (reverseResult != PerformanceDeckOwner::Result::applied) return Result::busy;
            control.playing.store(false, std::memory_order_release);
            control.rate.store(1.0f, std::memory_order_release);
            return Result::applied;
        }

        const double magnitude = std::clamp(std::abs(signedSpeed),
                                            minAudibleSpeed, maxAudibleSpeed);
        const auto reverseResult = owner.setReverseEnabled(signedSpeed < 0.0);
        if (reverseResult != PerformanceDeckOwner::Result::applied) return Result::busy;
        control.rate.store(static_cast<float>(magnitude), std::memory_order_release);
        control.playing.store(true, std::memory_order_release);
        return Result::applied;
    }

    // Precise platter relocation for controller touch strips / jog wheels.
    // The target is derived from the audible cursor, clamped to the current
    // track and published through the existing last-request-wins seek mailbox.
    // The next Engine block applies the existing prepared seek transition.
    [[nodiscard]] Result moveBySeconds(double deltaSeconds) noexcept {
        if (!gestureActive) return Result::notActive;
        if (!std::isfinite(deltaSeconds)) return Result::invalidRequest;
        if (conflictingOwnership()) return Result::busy;
        const double duration = engine.meter(deck).duration.load(std::memory_order_acquire);
        const double audible = engine.meter(deck).audiblePosition.load(std::memory_order_acquire);
        if (!std::isfinite(duration) || duration <= 0.0 || !std::isfinite(audible))
            return Result::trackUnavailable;

        const double upper = std::nextafter(duration, 0.0);
        const double target = std::clamp(audible + deltaSeconds, 0.0, upper);
        engine.control(deck).seek.store(target / duration, std::memory_order_release);
        return Result::applied;
    }

    // Normal platter release restores the pre-gesture play/rate snapshot. If a
    // different transport owner appeared during the gesture, do not resume the
    // old playing state underneath it: clear only scratch Reverse, preserve a
    // foreign Slip/loop state, restore rate, stop and report busy.
    [[nodiscard]] Result end() noexcept {
        if (!gestureActive) return Result::notActive;
        const bool conflict = conflictingOwnership();
        const auto reverseResult = owner.setReverseEnabled(false);
        if (reverseResult != PerformanceDeckOwner::Result::applied) return Result::busy;
        auto& control = engine.control(deck);
        control.rate.store(savedRate, std::memory_order_release);
        control.playing.store(conflict ? false : savedPlaying, std::memory_order_release);
        gestureActive = false;
        return conflict ? Result::busy : Result::applied;
    }

    // Forced abort for clip replacement, controller disconnect, dialog/device
    // teardown or shutdown. It never resumes transport, but it does restore the
    // user's pre-gesture rate. setReverseEnabled(false) intentionally preserves
    // a Slip state that may have been asserted by another message/controller
    // owner after the scratch began.
    [[nodiscard]] Result cancel() noexcept {
        if (!gestureActive) return Result::notActive;
        const auto reverseResult = owner.setReverseEnabled(false);
        if (reverseResult != PerformanceDeckOwner::Result::applied) return Result::busy;
        auto& control = engine.control(deck);
        control.rate.store(savedRate, std::memory_order_release);
        control.playing.store(false, std::memory_order_release);
        gestureActive = false;
        return Result::applied;
    }

private:
    [[nodiscard]] bool conflictingOwnership() const noexcept {
        return owner.beatLoopActive() || engine.loopRegionEnabled(deck) || owner.slipEnabled();
    }

    Engine& engine;
    PerformanceDeckOwner& owner;
    std::size_t deck = 0;
    bool gestureActive = false;
    bool savedPlaying = false;
    float savedRate = 1.0f;
};

} // namespace broke
