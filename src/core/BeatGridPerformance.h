// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "BeatAnalysis.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace broke {

// These helpers turn a reviewed BeatGrid into deterministic performance plans.
// They intentionally do not mutate transport state and are designed for the
// owner/UI/control side of the application, not for the realtime callback.
enum class QuantizeDirection {
    previous,
    nearest,
    next,
};

struct BeatLoopPlan final {
    bool valid = false;
    double startSeconds = 0.0;
    double endSeconds = 0.0;
    double startBeat = 0.0;
    double lengthBeats = 0.0;
};

struct BeatSyncPlan final {
    bool valid = false;
    double followerRate = 1.0;
    double followerTargetSeconds = 0.0;
    double followerTargetBeat = 0.0;
    double masterBeat = 0.0;
    double phaseErrorBeats = 0.0;
    double followerBpm = 0.0;
    double masterBpm = 0.0;
};

[[nodiscard]] inline double beatGridBpmAtTime(const BeatGrid& grid, double seconds) noexcept {
    if (!grid.valid() || !std::isfinite(seconds) || grid.segments().empty())
        return std::numeric_limits<double>::quiet_NaN();

    const auto& segments = grid.segments();
    double bpm = segments.front().bpm;
    for (const auto& segment : segments) {
        if (segment.startSeconds > seconds) break;
        bpm = segment.bpm;
    }
    return std::isfinite(bpm) && bpm > 0.0
        ? bpm
        : std::numeric_limits<double>::quiet_NaN();
}

[[nodiscard]] inline double quantizedBeat(const BeatGrid& grid,
                                          double seconds,
                                          double beatStep = 1.0,
                                          QuantizeDirection direction = QuantizeDirection::nearest) noexcept {
    if (!grid.valid() || !std::isfinite(seconds) || !std::isfinite(beatStep) || beatStep <= 0.0)
        return std::numeric_limits<double>::quiet_NaN();

    const double beat = grid.beatAtTime(seconds);
    if (!std::isfinite(beat)) return std::numeric_limits<double>::quiet_NaN();

    const double scaled = beat / beatStep;
    constexpr double edgeTolerance = 1.0e-10;
    double snapped = 0.0;
    switch (direction) {
        case QuantizeDirection::previous:
            snapped = std::floor(scaled + edgeTolerance);
            break;
        case QuantizeDirection::nearest:
            snapped = std::round(scaled);
            break;
        case QuantizeDirection::next:
            snapped = std::ceil(scaled - edgeTolerance);
            break;
    }
    return snapped * beatStep;
}

[[nodiscard]] inline double quantizedBeatTime(const BeatGrid& grid,
                                              double seconds,
                                              double beatStep = 1.0,
                                              QuantizeDirection direction = QuantizeDirection::nearest) noexcept {
    const double beat = quantizedBeat(grid, seconds, beatStep, direction);
    return std::isfinite(beat)
        ? grid.timeAtBeat(beat)
        : std::numeric_limits<double>::quiet_NaN();
}

// Plan a musical loop using beat-space endpoints. This remains correct when a
// loop crosses a variable-tempo segment because both endpoints are resolved by
// the BeatGrid rather than by multiplying one BPM-derived duration.
[[nodiscard]] inline BeatLoopPlan planBeatLoop(const BeatGrid& grid,
                                               double currentSeconds,
                                               double lengthBeats,
                                               QuantizeDirection direction = QuantizeDirection::previous) noexcept {
    BeatLoopPlan plan;
    if (!grid.valid() || !std::isfinite(currentSeconds) || currentSeconds < 0.0
        || !std::isfinite(lengthBeats) || lengthBeats <= 0.0 || lengthBeats > 256.0) {
        return plan;
    }

    const double startBeat = quantizedBeat(grid, currentSeconds, 1.0, direction);
    if (!std::isfinite(startBeat)) return plan;
    const double start = grid.timeAtBeat(startBeat);
    const double end = grid.timeAtBeat(startBeat + lengthBeats);
    if (!std::isfinite(start) || !std::isfinite(end) || start < 0.0 || end <= start)
        return plan;

    plan.valid = true;
    plan.startSeconds = start;
    plan.endSeconds = end;
    plan.startBeat = startBeat;
    plan.lengthBeats = lengthBeats;
    return plan;
}

// Build a one-shot deck-sync plan from two reviewed grids. The returned rate is
// a transport command candidate, not an instruction to mutate audio state in
// the callback. Phase alignment is expressed as a target follower time so the
// owner layer can decide whether/when a seek is musically acceptable.
[[nodiscard]] inline BeatSyncPlan planBeatSync(const BeatGrid& follower,
                                               double followerSeconds,
                                               const BeatGrid& master,
                                               double masterSeconds,
                                               double maxRateDelta = 0.5) noexcept {
    BeatSyncPlan plan;
    if (!follower.valid() || !master.valid()
        || !std::isfinite(followerSeconds) || followerSeconds < 0.0
        || !std::isfinite(masterSeconds) || masterSeconds < 0.0
        || !std::isfinite(maxRateDelta) || maxRateDelta < 0.0 || maxRateDelta > 0.5) {
        return plan;
    }

    const double followerBpm = beatGridBpmAtTime(follower, followerSeconds);
    const double masterBpm = beatGridBpmAtTime(master, masterSeconds);
    const double followerBeat = follower.beatAtTime(followerSeconds);
    const double masterBeat = master.beatAtTime(masterSeconds);
    if (!std::isfinite(followerBpm) || !std::isfinite(masterBpm)
        || !std::isfinite(followerBeat) || !std::isfinite(masterBeat)
        || followerBpm <= 0.0 || masterBpm <= 0.0) {
        return plan;
    }

    const double rate = masterBpm / followerBpm;
    const double minRate = 1.0 - maxRateDelta;
    const double maxRate = 1.0 + maxRateDelta;
    if (!std::isfinite(rate) || rate < minRate || rate > maxRate)
        return plan;

    const double masterPhase = masterBeat - std::floor(masterBeat);
    const double targetBeat = std::round(followerBeat - masterPhase) + masterPhase;
    const double targetSeconds = follower.timeAtBeat(targetBeat);
    if (!std::isfinite(targetSeconds) || targetSeconds < 0.0)
        return plan;

    plan.valid = true;
    plan.followerRate = rate;
    plan.followerTargetSeconds = targetSeconds;
    plan.followerTargetBeat = targetBeat;
    plan.masterBeat = masterBeat;
    plan.phaseErrorBeats = targetBeat - followerBeat;
    plan.followerBpm = followerBpm;
    plan.masterBpm = masterBpm;
    return plan;
}

} // namespace broke
