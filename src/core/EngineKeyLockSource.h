// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "core/DeckPlaybackSelector.h"
#include "core/Engine.h"

#include <cmath>

namespace broke {

// Optional M2 adapter that connects the qualified deck-owned key-lock selector
// to Engine's raw deck-source boundary. The adapter itself owns no Clip and is
// intentionally fail-closed: a changed clip, loop state or playback-rate target
// is rejected until the owner restages off the audio callback. Engine then uses
// its built-in production rate converter immediately for that block.
//
// Lifecycle contract:
//  1. prepare() while audio is stopped;
//  2. stage() against the exact immutable Clip/cursor/loop/control snapshot;
//  3. install this non-owning object with Engine::setDeckSourceRenderer() while
//     audio is stopped;
//  4. keep the adapter alive until it is removed after audio stops again.
//
// No GUI control is implied by this adapter. Physical latency/listening quality
// remain separate qualification gates.
class EngineKeyLockSource final : public DeckSourceRenderer {
public:
    using ControlSnapshot = DeckPlaybackSelector::ControlSnapshot;
    using RenderPath = DeckPlaybackSelector::RenderPath;
    using FallbackReason = DeckPlaybackSelector::FallbackReason;

    [[nodiscard]] bool prepare(double sourceSampleRate, double deviceSampleRate,
                               int maxDeviceFrames, double maxPlaybackRate = 4.0,
                               bool splitComputation = true) {
        stagedClip = nullptr;
        stagedLoop = false;
        lastEngineAccepted = false;
        return selector.prepare(sourceSampleRate, deviceSampleRate, maxDeviceFrames,
                                maxPlaybackRate, splitComputation);
    }

    [[nodiscard]] bool stage(const Clip& clip, double cursor, bool loop,
                             const ControlSnapshot& controls) noexcept {
        stagedClip = nullptr;
        lastEngineAccepted = false;
        if (!selector.stage(clip, cursor, loop, controls)) return false;
        if (!controls.enabled) return true;
        stagedClip = &clip;
        stagedLoop = loop;
        return true;
    }

    void disarm() noexcept {
        selector.disarm();
        stagedClip = nullptr;
        lastEngineAccepted = false;
    }

    [[nodiscard]] bool render(const Clip& clip, double cursor, bool loop,
                              double playbackRate,
                              float* outputLeft, float* outputRight,
                              int deviceFrames, double& nextTransportCursor,
                              double& nextAudibleCursor) noexcept override {
        lastEngineAccepted = false;
        if (stagedClip != &clip || loop != stagedLoop || !selector.armed()
            || !std::isfinite(playbackRate)
            || !sameRate(playbackRate, selector.controls().playbackRate)) {
            return false;
        }
        if (!selector.render(clip, cursor, loop, outputLeft, outputRight,
                             deviceFrames, nextTransportCursor, nextAudibleCursor)) {
            return false;
        }
        lastEngineAccepted = true;
        return true;
    }

    [[nodiscard]] bool prepared() const noexcept { return selector.prepared(); }
    [[nodiscard]] bool armed() const noexcept { return selector.armed(); }
    [[nodiscard]] bool needsStage() const noexcept { return selector.needsStage(); }
    [[nodiscard]] bool lastEngineRenderAccepted() const noexcept { return lastEngineAccepted; }
    [[nodiscard]] RenderPath lastRenderPath() const noexcept { return selector.lastRenderPath(); }
    [[nodiscard]] FallbackReason lastFallbackReason() const noexcept {
        return selector.lastFallbackReason();
    }
    [[nodiscard]] double transportCursor() const noexcept { return selector.transportCursor(); }
    [[nodiscard]] double audibleCursor() const noexcept { return selector.audibleCursor(); }
    [[nodiscard]] int reportedDeviceOutputLatencyFrames() const noexcept {
        return selector.reportedDeviceOutputLatencyFrames();
    }
    [[nodiscard]] const ControlSnapshot& controls() const noexcept { return selector.controls(); }

private:
    [[nodiscard]] static bool sameRate(double a, double b) noexcept {
        constexpr double absoluteTolerance = 1.0e-7;
        constexpr double relativeTolerance = 1.0e-6;
        const double scale = std::max(1.0, std::max(std::abs(a), std::abs(b)));
        return std::abs(a - b) <= std::max(absoluteTolerance, relativeTolerance * scale);
    }

    DeckPlaybackSelector selector;
    const Clip* stagedClip = nullptr;
    bool stagedLoop = false;
    bool lastEngineAccepted = false;
};

} // namespace broke
