// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "core/TimeStretchEngineBridge.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace broke {

// M2 deck-owned integration candidate above TimeStretchEngineBridge.
//
// The selector keeps ordinary production-style playback as the immediate
// fallback while owning key-lock staging, selected-path diagnostics, audible
// transport metadata and a short path-transition de-click. prepare() and
// stage() are non-realtime lifecycle operations and must never race render().
// render() is allocation-free after prepare() and does no I/O, decoding,
// logging, locking or processor re-prime work.
//
// This class is deliberately not wired into Engine::process() yet. It proves
// the deck-level ownership contract required before a user-visible key-lock
// control can be exposed.
class DeckPlaybackSelector final {
public:
    using RenderPath = TimeStretchEngineBridge::RenderPath;
    using FallbackReason = TimeStretchEngineBridge::FallbackReason;
    using ControlSnapshot = TimeStretchEngineBridge::ControlSnapshot;

    [[nodiscard]] bool prepare(double sourceSampleRate, double deviceSampleRate,
                               int maxDeviceFrames, double maxPlaybackRate = 4.0,
                               bool splitComputation = true) {
        ready = false;
        if (!bridge.prepare(sourceSampleRate, deviceSampleRate, maxDeviceFrames,
                            maxPlaybackRate, splitComputation)) {
            return false;
        }
        maxFrames = maxDeviceFrames;
        transitionFrames = std::max(1, static_cast<int>(std::lround(deviceSampleRate * 0.005)));
        transitionRemaining = 0;
        transitionFrom.fill(0.0f);
        lastOutput.fill(0.0f);
        hasLastOutput = false;
        lastPathValue = RenderPath::fallback;
        lastReasonValue = FallbackReason::disabled;
        stagedControls = {};
        keyLockArmed = false;
        lastTransport = 0.0;
        lastAudible = 0.0;
        ready = true;
        return true;
    }

    // Transactional lifecycle handoff. The caller owns serialization and must
    // invoke this outside the audio callback. A failed stage leaves key lock
    // disarmed and the underlying bridge fail-closed to fallback while keeping
    // the last valid rate/pitch metadata for the deterministic fallback path.
    [[nodiscard]] bool stage(const Clip& clip, double cursor, bool loop,
                             const ControlSnapshot& controls) noexcept {
        if (!ready || !bridge.configureAndPrime(clip, cursor, loop, controls)) {
            stagedControls.enabled = false;
            keyLockArmed = false;
            return false;
        }
        stagedControls = controls;
        keyLockArmed = controls.enabled;
        return true;
    }

    // Explicit non-realtime disarm. The next render remains production
    // fallback and receives the same path-transition de-click as any other
    // stretch/fallback switch.
    void disarm() noexcept {
        bridge.setEnabled(false);
        bridge.reset();
        stagedControls.enabled = false;
        keyLockArmed = false;
    }

    [[nodiscard]] bool render(const Clip& clip, double cursor, bool loop,
                              float* outputLeft, float* outputRight,
                              int deviceFrames, double& nextTransportCursor,
                              double& nextAudibleCursor) noexcept {
        if (!ready || outputLeft == nullptr || outputRight == nullptr
            || deviceFrames <= 0 || deviceFrames > maxFrames) {
            return false;
        }

        double nextTransport = cursor;
        double nextAudible = cursor;
        if (!bridge.render(clip, cursor, loop, outputLeft, outputRight,
                           deviceFrames, nextTransport, nextAudible)) {
            return false;
        }

        const auto selectedPath = bridge.lastRenderPath();
        const auto selectedReason = bridge.lastFallbackReason();
        if (hasLastOutput && selectedPath != lastPathValue) {
            transitionFrom = lastOutput;
            transitionRemaining = transitionFrames;
        }

        for (int frame = 0; frame < deviceFrames; ++frame) {
            if (transitionRemaining > 0) {
                const float mix = 1.0f - static_cast<float>(transitionRemaining)
                    / static_cast<float>(transitionFrames);
                const float dry = 1.0f - mix;
                outputLeft[frame] = finite(transitionFrom[0] * dry + outputLeft[frame] * mix);
                outputRight[frame] = finite(transitionFrom[1] * dry + outputRight[frame] * mix);
                --transitionRemaining;
            } else {
                outputLeft[frame] = finite(outputLeft[frame]);
                outputRight[frame] = finite(outputRight[frame]);
            }
        }

        lastOutput[0] = outputLeft[deviceFrames - 1];
        lastOutput[1] = outputRight[deviceFrames - 1];
        hasLastOutput = true;
        lastPathValue = selectedPath;
        lastReasonValue = selectedReason;
        lastTransport = nextTransport;
        lastAudible = nextAudible;
        nextTransportCursor = nextTransport;
        nextAudibleCursor = nextAudible;
        return true;
    }

    [[nodiscard]] bool prepared() const noexcept { return ready && bridge.prepared(); }
    [[nodiscard]] bool armed() const noexcept { return keyLockArmed; }
    [[nodiscard]] bool needsStage() const noexcept { return keyLockArmed && bridge.needsPrime(); }
    [[nodiscard]] RenderPath lastRenderPath() const noexcept { return lastPathValue; }
    [[nodiscard]] FallbackReason lastFallbackReason() const noexcept { return lastReasonValue; }
    [[nodiscard]] const ControlSnapshot& controls() const noexcept { return stagedControls; }
    [[nodiscard]] double transportCursor() const noexcept { return lastTransport; }
    [[nodiscard]] double audibleCursor() const noexcept { return lastAudible; }
    [[nodiscard]] int transitionFramesRemaining() const noexcept { return transitionRemaining; }
    [[nodiscard]] int reportedDeviceOutputLatencyFrames() const noexcept {
        return bridge.reportedDeviceOutputLatencyFrames();
    }

private:
    [[nodiscard]] static float finite(float value) noexcept {
        return std::isfinite(value) ? value : 0.0f;
    }

    TimeStretchEngineBridge bridge;
    ControlSnapshot stagedControls{};
    std::array<float, 2> transitionFrom{};
    std::array<float, 2> lastOutput{};
    double lastTransport = 0.0;
    double lastAudible = 0.0;
    int maxFrames = 0;
    int transitionFrames = 1;
    int transitionRemaining = 0;
    bool ready = false;
    bool keyLockArmed = false;
    bool hasLastOutput = false;
    RenderPath lastPathValue = RenderPath::fallback;
    FallbackReason lastReasonValue = FallbackReason::disabled;
};

} // namespace broke
