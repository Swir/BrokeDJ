// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "BeatGridPerformance.h"
#include "Engine.h"

#include <array>
#include <cmath>
#include <cstddef>

namespace broke {

// Message/control-thread owner for reviewed-grid performance actions. The class
// never decodes, allocates on the audio callback, or performs file/network I/O.
// It translates explicit UI/controller intent into the existing Engine atomics
// and production beat-loop region boundary while keeping whole-track LOOP a
// separate, unchanged mode.
class PerformanceDeckOwner final {
public:
    static constexpr std::size_t hotCueCount = 8;

    enum class Result {
        applied,
        invalidDeck,
        gridUnavailable,
        invalidRequest,
        trackUnavailable,
        outsideTrack,
        rendererBusy,
        cueUnavailable
    };

    struct HotCue final {
        bool set = false;
        double seconds = 0.0;
        bool quantized = false;
        double beatStep = 1.0;
    };

    using HotCueBank = std::array<HotCue, hotCueCount>;

    PerformanceDeckOwner(Engine& targetEngine, std::size_t deckIndex) noexcept
        : engine(targetEngine), deck(deckIndex) {}

    [[nodiscard]] bool validDeck() const noexcept { return deck < deckCount; }
    [[nodiscard]] bool hasReviewedGrid() const noexcept { return gridReady && grid.valid(); }
    [[nodiscard]] bool beatLoopActive() const noexcept {
        return activeBeatLoopBeats > 0.0 && validDeck()
            && engine.loopRegionEnabled(deck)
            && engine.control(deck).loop.load(std::memory_order_acquire);
    }
    [[nodiscard]] double beatLoopLength() const noexcept { return activeBeatLoopBeats; }
    [[nodiscard]] const BeatGrid& reviewedGrid() const noexcept { return grid; }

    // Replacing a reviewed grid invalidates an armed musical loop rather than
    // silently retaining source-time bounds derived from the previous grid.
    [[nodiscard]] Result setReviewedGrid(const BeatGrid& reviewed) {
        if (!validDeck()) return Result::invalidDeck;
        if (!reviewed.valid()) return Result::gridUnavailable;
        if (activeBeatLoopBeats > 0.0) disarmLoop();
        grid = reviewed;
        gridReady = true;
        return Result::applied;
    }

    // Reviewed tempo-map edits are owned here rather than mutating a grid behind
    // the performance boundary. Every operation edits a candidate copy first;
    // only a fully valid candidate replaces the live reviewed snapshot. A valid
    // replacement deliberately disarms a beat-derived loop because its source-
    // time bounds were calculated from the old map. Invalid edits leave both the
    // map and active loop untouched.
    [[nodiscard]] Result setReviewedSegmentBpm(std::size_t index, double bpm) {
        if (!validDeck()) return Result::invalidDeck;
        if (!hasReviewedGrid()) return Result::gridUnavailable;
        auto candidate = grid;
        if (!candidate.setSegmentBpm(index, bpm)) return Result::invalidRequest;
        return setReviewedGrid(candidate);
    }

    [[nodiscard]] Result insertReviewedTempoChange(double beat, double bpm) {
        if (!validDeck()) return Result::invalidDeck;
        if (!hasReviewedGrid()) return Result::gridUnavailable;
        auto candidate = grid;
        if (!candidate.insertTempoChangeAtBeat(beat, bpm)) return Result::invalidRequest;
        return setReviewedGrid(candidate);
    }

    [[nodiscard]] Result moveReviewedTempoChange(std::size_t index, double beat) {
        if (!validDeck()) return Result::invalidDeck;
        if (!hasReviewedGrid()) return Result::gridUnavailable;
        auto candidate = grid;
        if (!candidate.moveTempoChangeToBeat(index, beat)) return Result::invalidRequest;
        return setReviewedGrid(candidate);
    }

    [[nodiscard]] Result replaceReviewedTempoChange(std::size_t index, double beat, double bpm) {
        if (!validDeck()) return Result::invalidDeck;
        if (!hasReviewedGrid()) return Result::gridUnavailable;
        auto candidate = grid;
        if (!candidate.replaceTempoChange(index, beat, bpm)) return Result::invalidRequest;
        return setReviewedGrid(candidate);
    }

    [[nodiscard]] Result removeReviewedTempoChange(std::size_t index) {
        if (!validDeck()) return Result::invalidDeck;
        if (!hasReviewedGrid()) return Result::gridUnavailable;
        auto candidate = grid;
        if (!candidate.removeTempoChange(index)) return Result::invalidRequest;
        return setReviewedGrid(candidate);
    }

    void clearReviewedGrid() noexcept {
        if (activeBeatLoopBeats > 0.0) disarmLoop();
        grid = BeatGrid{};
        gridReady = false;
    }

    // New immutable clip identity: no stale loop region or cue can carry over.
    void resetForClip() noexcept {
        if (validDeck()) {
            engine.clearLoopRegion(deck);
            engine.control(deck).loop.store(false, std::memory_order_release);
            engine.control(deck).seek.store(-1.0, std::memory_order_release);
        }
        activeBeatLoopBeats = 0.0;
        grid = BeatGrid{};
        gridReady = false;
        clearHotCues();
    }

    [[nodiscard]] Result setWholeTrackLoop(bool enabled) noexcept {
        if (!validDeck()) return Result::invalidDeck;
        engine.clearLoopRegion(deck);
        activeBeatLoopBeats = 0.0;
        engine.control(deck).loop.store(enabled, std::memory_order_release);
        return Result::applied;
    }

    [[nodiscard]] Result armBeatLoopAt(double cursorSeconds, double trackDurationSeconds,
                                       double lengthBeats,
                                       QuantizeDirection direction = QuantizeDirection::previous) noexcept {
        if (!validDeck()) return Result::invalidDeck;
        if (!hasReviewedGrid()) return Result::gridUnavailable;
        if (!std::isfinite(cursorSeconds) || !std::isfinite(trackDurationSeconds)
            || !std::isfinite(lengthBeats) || cursorSeconds < 0.0
            || trackDurationSeconds <= 0.0 || lengthBeats <= 0.0 || lengthBeats > 256.0) {
            return Result::invalidRequest;
        }
        if (cursorSeconds >= trackDurationSeconds) return Result::outsideTrack;

        const auto plan = planBeatLoop(grid, cursorSeconds, lengthBeats, direction);
        if (!plan.valid || !std::isfinite(plan.startSeconds) || !std::isfinite(plan.endSeconds)
            || plan.startSeconds < 0.0 || plan.endSeconds <= plan.startSeconds) {
            return Result::invalidRequest;
        }
        if (plan.endSeconds > trackDurationSeconds + 1.0e-9)
            return Result::outsideTrack;

        // Transactional ordering: do not change LOOP state until Engine accepts
        // the complete reviewed source-time region. A key-lock/research renderer
        // or other external owner therefore fails closed without UI state drift.
        if (!engine.setLoopRegionSeconds(deck, plan.startSeconds, plan.endSeconds))
            return Result::rendererBusy;

        activeBeatLoopBeats = lengthBeats;
        engine.control(deck).loop.store(true, std::memory_order_release);
        return Result::applied;
    }

    [[nodiscard]] Result armBeatLoopFromTransport(double lengthBeats,
                                                  QuantizeDirection direction = QuantizeDirection::previous) noexcept {
        if (!validDeck()) return Result::invalidDeck;
        const auto cursor = engine.meter(deck).position.load(std::memory_order_acquire);
        const auto duration = engine.meter(deck).duration.load(std::memory_order_acquire);
        if (!std::isfinite(duration) || duration <= 0.0) return Result::trackUnavailable;
        return armBeatLoopAt(cursor, duration, lengthBeats, direction);
    }

    void disarmLoop() noexcept {
        if (!validDeck()) return;
        engine.clearLoopRegion(deck);
        activeBeatLoopBeats = 0.0;
        engine.control(deck).loop.store(false, std::memory_order_release);
    }

    [[nodiscard]] Result storeHotCueAt(std::size_t slot, double cursorSeconds,
                                       double trackDurationSeconds, bool quantize,
                                       double beatStep = 1.0,
                                       QuantizeDirection direction = QuantizeDirection::nearest) noexcept {
        if (!validDeck()) return Result::invalidDeck;
        if (slot >= hotCueCount || !std::isfinite(cursorSeconds)
            || !std::isfinite(trackDurationSeconds) || !std::isfinite(beatStep)
            || cursorSeconds < 0.0 || trackDurationSeconds <= 0.0
            || beatStep <= 0.0 || beatStep > 64.0) {
            return Result::invalidRequest;
        }
        if (cursorSeconds >= trackDurationSeconds) return Result::outsideTrack;

        double target = cursorSeconds;
        if (quantize) {
            if (!hasReviewedGrid()) return Result::gridUnavailable;
            target = quantizedBeatTime(grid, cursorSeconds, beatStep, direction);
            if (!std::isfinite(target)) return Result::invalidRequest;
        }
        if (target < 0.0 || target >= trackDurationSeconds) return Result::outsideTrack;

        hotCues[slot] = {true, target, quantize, beatStep};
        return Result::applied;
    }

    [[nodiscard]] Result storeHotCueFromTransport(std::size_t slot, bool quantize = true,
                                                  double beatStep = 1.0,
                                                  QuantizeDirection direction = QuantizeDirection::nearest) noexcept {
        if (!validDeck()) return Result::invalidDeck;
        const auto cursor = engine.meter(deck).position.load(std::memory_order_acquire);
        const auto duration = engine.meter(deck).duration.load(std::memory_order_acquire);
        if (!std::isfinite(duration) || duration <= 0.0) return Result::trackUnavailable;
        return storeHotCueAt(slot, cursor, duration, quantize, beatStep, direction);
    }

    // Persistence/session restoration is exact source-time restoration, not a
    // second quantization pass. Validate the complete bank first so a corrupt or
    // stale record cannot partially replace an already-valid in-memory bank.
    [[nodiscard]] Result restoreHotCueBank(const HotCueBank& snapshot,
                                           double trackDurationSeconds) noexcept {
        if (!validDeck()) return Result::invalidDeck;
        if (!std::isfinite(trackDurationSeconds) || trackDurationSeconds <= 0.0)
            return Result::trackUnavailable;

        HotCueBank validated{};
        for (std::size_t slot = 0; slot < snapshot.size(); ++slot) {
            const auto& cue = snapshot[slot];
            if (!cue.set) continue;
            if (!std::isfinite(cue.seconds) || !std::isfinite(cue.beatStep)
                || cue.seconds < 0.0 || cue.beatStep <= 0.0 || cue.beatStep > 64.0) {
                return Result::invalidRequest;
            }
            if (cue.seconds >= trackDurationSeconds) return Result::outsideTrack;
            validated[slot] = cue;
        }

        hotCues = validated;
        return Result::applied;
    }

    [[nodiscard]] HotCueBank hotCueBank() const noexcept { return hotCues; }

    [[nodiscard]] Result triggerHotCue(std::size_t slot, double trackDurationSeconds) noexcept {
        if (!validDeck()) return Result::invalidDeck;
        if (slot >= hotCueCount || !hotCues[slot].set) return Result::cueUnavailable;
        if (!std::isfinite(trackDurationSeconds) || trackDurationSeconds <= 0.0)
            return Result::trackUnavailable;

        const auto target = hotCues[slot].seconds;
        if (!std::isfinite(target) || target < 0.0 || target >= trackDurationSeconds)
            return Result::outsideTrack;

        // Initial fail-safe contract: jumping to a cue exits an active beat loop
        // instead of letting a stale source-time loop reinterpret the new cursor.
        // Whole-track LOOP is intentionally preserved.
        if (activeBeatLoopBeats > 0.0) disarmLoop();

        engine.control(deck).seek.store(target / trackDurationSeconds, std::memory_order_release);
        return Result::applied;
    }

    [[nodiscard]] Result triggerHotCueFromTransport(std::size_t slot) noexcept {
        if (!validDeck()) return Result::invalidDeck;
        const auto duration = engine.meter(deck).duration.load(std::memory_order_acquire);
        return triggerHotCue(slot, duration);
    }

    // Preserve the current fractional beat phase while moving by an explicit
    // musical distance. An active beat-derived loop is exited before the seek;
    // explicit whole-track LOOP remains unchanged.
    [[nodiscard]] Result jumpBeatsAt(double cursorSeconds, double trackDurationSeconds,
                                     double deltaBeats) noexcept {
        if (!validDeck()) return Result::invalidDeck;
        if (!hasReviewedGrid()) return Result::gridUnavailable;
        if (!std::isfinite(cursorSeconds) || !std::isfinite(trackDurationSeconds)
            || !std::isfinite(deltaBeats) || cursorSeconds < 0.0
            || trackDurationSeconds <= 0.0 || deltaBeats == 0.0
            || std::abs(deltaBeats) > 256.0) {
            return Result::invalidRequest;
        }
        if (cursorSeconds >= trackDurationSeconds) return Result::outsideTrack;

        const double sourceBeat = grid.beatAtTime(cursorSeconds);
        if (!std::isfinite(sourceBeat)) return Result::invalidRequest;
        const double targetSeconds = grid.timeAtBeat(sourceBeat + deltaBeats);
        if (!std::isfinite(targetSeconds) || targetSeconds < 0.0
            || targetSeconds >= trackDurationSeconds) {
            return Result::outsideTrack;
        }

        if (activeBeatLoopBeats > 0.0) disarmLoop();
        engine.control(deck).seek.store(targetSeconds / trackDurationSeconds,
                                        std::memory_order_release);
        return Result::applied;
    }

    [[nodiscard]] Result jumpBeatsFromTransport(double deltaBeats) noexcept {
        if (!validDeck()) return Result::invalidDeck;
        const auto cursor = engine.meter(deck).position.load(std::memory_order_acquire);
        const auto duration = engine.meter(deck).duration.load(std::memory_order_acquire);
        if (!std::isfinite(duration) || duration <= 0.0) return Result::trackUnavailable;
        return jumpBeatsAt(cursor, duration, deltaBeats);
    }

    // Apply one bounded sync decision to this follower deck. Both grids must be
    // reviewed snapshots from owners attached to the same Engine. Validation is
    // completed before controls mutate. Master rate is sampled once so the
    // follower matches the master's effective tempo, not merely its source BPM.
    // The current production Engine de-clicks the resulting seek/rate
    // discontinuity; this is not continuous phase-lock.
    [[nodiscard]] Result syncToAt(const PerformanceDeckOwner& master,
                                  double followerSeconds, double followerDurationSeconds,
                                  double masterSeconds, double masterDurationSeconds,
                                  double maxRateDelta = 0.20,
                                  double maxPhaseCorrectionBeats = 0.50) noexcept {
        if (!validDeck() || !master.validDeck()) return Result::invalidDeck;
        if (&engine != &master.engine || deck == master.deck) return Result::invalidRequest;
        if (!hasReviewedGrid() || !master.hasReviewedGrid()) return Result::gridUnavailable;
        if (!std::isfinite(followerSeconds) || !std::isfinite(followerDurationSeconds)
            || !std::isfinite(masterSeconds) || !std::isfinite(masterDurationSeconds)
            || !std::isfinite(maxRateDelta) || !std::isfinite(maxPhaseCorrectionBeats)
            || followerSeconds < 0.0 || masterSeconds < 0.0
            || followerDurationSeconds <= 0.0 || masterDurationSeconds <= 0.0
            || maxRateDelta < 0.0 || maxRateDelta > 0.50
            || maxPhaseCorrectionBeats < 0.0 || maxPhaseCorrectionBeats > 0.50) {
            return Result::invalidRequest;
        }
        if (followerSeconds >= followerDurationSeconds || masterSeconds >= masterDurationSeconds)
            return Result::outsideTrack;

        const double masterPlaybackRate = static_cast<double>(
            engine.control(master.deck).rate.load(std::memory_order_acquire));
        if (!std::isfinite(masterPlaybackRate)
            || masterPlaybackRate < 0.5 || masterPlaybackRate > 1.5) {
            return Result::invalidRequest;
        }

        const auto plan = planBeatSync(grid, followerSeconds,
                                       master.grid, masterSeconds,
                                       maxRateDelta, masterPlaybackRate);
        if (!plan.valid || !std::isfinite(plan.followerRate)
            || !std::isfinite(plan.followerTargetSeconds)
            || !std::isfinite(plan.phaseErrorBeats)) {
            return Result::invalidRequest;
        }
        if (plan.followerTargetSeconds < 0.0
            || plan.followerTargetSeconds >= followerDurationSeconds) {
            return Result::outsideTrack;
        }
        if (std::abs(plan.phaseErrorBeats) > maxPhaseCorrectionBeats + 1.0e-12)
            return Result::invalidRequest;
        if (plan.followerRate < 0.5 || plan.followerRate > 1.5)
            return Result::invalidRequest;

        if (activeBeatLoopBeats > 0.0) disarmLoop();
        auto& control = engine.control(deck);
        control.rate.store(static_cast<float>(plan.followerRate), std::memory_order_release);
        if (std::abs(plan.phaseErrorBeats) > 1.0e-9) {
            control.seek.store(plan.followerTargetSeconds / followerDurationSeconds,
                               std::memory_order_release);
        }
        return Result::applied;
    }

    [[nodiscard]] Result syncToTransport(const PerformanceDeckOwner& master,
                                         double maxRateDelta = 0.20,
                                         double maxPhaseCorrectionBeats = 0.50) noexcept {
        if (!validDeck() || !master.validDeck()) return Result::invalidDeck;
        const auto followerSeconds = engine.meter(deck).position.load(std::memory_order_acquire);
        const auto followerDuration = engine.meter(deck).duration.load(std::memory_order_acquire);
        const auto masterSeconds = engine.meter(master.deck).position.load(std::memory_order_acquire);
        const auto masterDuration = engine.meter(master.deck).duration.load(std::memory_order_acquire);
        if (!std::isfinite(followerDuration) || followerDuration <= 0.0
            || !std::isfinite(masterDuration) || masterDuration <= 0.0) {
            return Result::trackUnavailable;
        }
        return syncToAt(master, followerSeconds, followerDuration,
                        masterSeconds, masterDuration,
                        maxRateDelta, maxPhaseCorrectionBeats);
    }

    void clearHotCue(std::size_t slot) noexcept {
        if (slot < hotCueCount) hotCues[slot] = {};
    }

    void clearHotCues() noexcept {
        for (auto& cue : hotCues) cue = {};
    }

    [[nodiscard]] const HotCue& hotCue(std::size_t slot) const noexcept {
        static const HotCue empty{};
        return slot < hotCueCount ? hotCues[slot] : empty;
    }

private:
    Engine& engine;
    std::size_t deck = deckCount;
    BeatGrid grid;
    bool gridReady = false;
    double activeBeatLoopBeats = 0.0;
    HotCueBank hotCues{};
};

} // namespace broke
