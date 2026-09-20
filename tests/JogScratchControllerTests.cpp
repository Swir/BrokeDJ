// SPDX-License-Identifier: AGPL-3.0-only
#include "core/JogScratchController.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {
int checks = 0;

void check(bool condition, const char* name) {
    ++checks;
    if (!condition) throw std::runtime_error(name);
}

std::unique_ptr<broke::Clip> makeClip(double seconds = 8.0, double sampleRate = 48000.0) {
    auto clip = std::make_unique<broke::Clip>();
    clip->sampleRate = sampleRate;
    const auto frames = static_cast<std::size_t>(std::llround(seconds * sampleRate));
    clip->left.resize(frames);
    clip->right.resize(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const float value = static_cast<float>(0.2 * std::sin(
            2.0 * 3.14159265358979323846 * 440.0 * static_cast<double>(i) / sampleRate));
        clip->left[i] = value;
        clip->right[i] = value;
    }
    return clip;
}

struct RenderBlock final {
    std::array<float, 256> left{};
    std::array<float, 256> right{};
    std::array<float*, 2> channels{left.data(), right.data()};
};

void process(broke::Engine& engine, RenderBlock& block) {
    engine.process(block.channels.data(), static_cast<int>(block.channels.size()),
                   static_cast<int>(block.left.size()));
}

void signedVelocityDrivesRealProductionTransport() {
    broke::Engine engine;
    engine.prepare(48000.0, 256);
    check(engine.submit(0, makeClip()), "scratch fixture submits");
    RenderBlock block;
    process(engine, block); // adopt clip and publish duration

    auto& control = engine.control(0);
    control.seek.store(0.5);
    control.rate.store(1.10f);
    control.playing.store(true);
    process(engine, block);

    broke::PerformanceDeckOwner owner(engine, 0);
    broke::JogScratchController scratch(engine, owner, 0);
    check(scratch.begin() == broke::JogScratchController::Result::applied,
          "scratch gesture acquires ordinary transport");
    check(scratch.active() && !control.playing.load(),
          "touching platter stops transport until signed velocity arrives");

    const double beforeReverse = engine.meter(0).audiblePosition.load();
    check(scratch.setVelocity(-1.25) == broke::JogScratchController::Result::applied,
          "negative platter velocity applies");
    check(control.reverse.load() && control.playing.load(),
          "negative velocity selects production reverse and plays");
    check(std::abs(static_cast<double>(control.rate.load()) - 1.25) < 1.0e-6,
          "negative velocity publishes bounded magnitude");
    for (int i = 0; i < 6; ++i) process(engine, block);
    const double afterReverse = engine.meter(0).audiblePosition.load();
    check(afterReverse < beforeReverse,
          "real Engine audible cursor moves backward during scratch gesture");

    check(scratch.setVelocity(0.75) == broke::JogScratchController::Result::applied,
          "positive platter velocity applies");
    check(!control.reverse.load() && control.playing.load(),
          "positive velocity returns production transport forward");
    const double beforeForward = engine.meter(0).audiblePosition.load();
    for (int i = 0; i < 6; ++i) process(engine, block);
    check(engine.meter(0).audiblePosition.load() > beforeForward,
          "real Engine audible cursor moves forward during jog gesture");

    const bool playingBeforeInvalid = control.playing.load();
    const bool reverseBeforeInvalid = control.reverse.load();
    const float rateBeforeInvalid = control.rate.load();
    check(scratch.setVelocity(2.0) == broke::JogScratchController::Result::invalidRequest,
          "velocity outside qualified engine envelope is rejected");
    check(control.playing.load() == playingBeforeInvalid
              && control.reverse.load() == reverseBeforeInvalid
              && control.rate.load() == rateBeforeInvalid,
          "invalid velocity fails closed without control drift");

    check(scratch.setVelocity(0.0) == broke::JogScratchController::Result::applied,
          "deadzone holds platter stationary");
    check(!control.playing.load() && !control.reverse.load(),
          "stationary platter pauses and clears transient reverse");

    check(scratch.end() == broke::JogScratchController::Result::applied,
          "platter release restores prior transport snapshot");
    check(!scratch.active() && control.playing.load() && !control.reverse.load(),
          "release resumes prior forward playing state");
    check(std::abs(static_cast<double>(control.rate.load()) - 1.10) < 1.0e-6,
          "release restores pre-gesture playback rate");
    check(scratch.end() == broke::JogScratchController::Result::notActive,
          "double release is rejected deterministically");
}

void relativeMoveUsesAudibleCursorAndTrackBounds() {
    broke::Engine engine;
    engine.prepare(48000.0, 256);
    check(engine.submit(1, makeClip(4.0)), "relative-move fixture submits");
    RenderBlock block;
    process(engine, block);

    auto& control = engine.control(1);
    control.seek.store(0.5);
    process(engine, block);

    broke::PerformanceDeckOwner owner(engine, 1);
    broke::JogScratchController scratch(engine, owner, 1);
    check(scratch.begin() == broke::JogScratchController::Result::applied,
          "relative-move gesture begins");
    const double before = engine.meter(1).audiblePosition.load();
    check(scratch.moveBySeconds(0.75) == broke::JogScratchController::Result::applied,
          "relative platter move publishes seek mailbox");
    process(engine, block);
    const double moved = engine.meter(1).audiblePosition.load();
    check(moved > before + 0.70 && moved < before + 0.80,
          "relative move follows audible cursor in source seconds");

    check(scratch.moveBySeconds(1000.0) == broke::JogScratchController::Result::applied,
          "large positive move is bounded instead of rejected");
    process(engine, block);
    check(engine.meter(1).audiblePosition.load() <= engine.meter(1).duration.load(),
          "positive move cannot seek beyond current track");

    check(scratch.moveBySeconds(-1000.0) == broke::JogScratchController::Result::applied,
          "large negative move is bounded instead of rejected");
    process(engine, block);
    check(engine.meter(1).audiblePosition.load() >= 0.0,
          "negative move cannot seek before track start");

    check(scratch.moveBySeconds(std::numeric_limits<double>::quiet_NaN())
              == broke::JogScratchController::Result::invalidRequest,
          "non-finite platter delta is rejected");
    check(scratch.end() == broke::JogScratchController::Result::applied,
          "relative-move gesture releases");
}

void conflictingOwnersFailClosed() {
    broke::Engine engine;
    engine.prepare(48000.0, 256);
    check(engine.submit(2, makeClip()), "ownership fixture submits");
    RenderBlock block;
    process(engine, block);

    broke::PerformanceDeckOwner owner(engine, 2);
    broke::JogScratchController scratch(engine, owner, 2);

    check(owner.setSlipEnabled(true) == broke::PerformanceDeckOwner::Result::applied,
          "slip ownership fixture arms");
    check(scratch.begin() == broke::JogScratchController::Result::busy,
          "scratch refuses existing slip ownership");
    check(owner.slipEnabled(), "rejected scratch does not destroy slip state");
    owner.clearReverseSlip();

    broke::BeatGrid grid;
    check(grid.reset(0.0, 120.0), "beat-loop ownership grid initializes");
    check(owner.setReviewedGrid(grid) == broke::PerformanceDeckOwner::Result::applied,
          "beat-loop ownership grid accepted");
    check(owner.armBeatLoopAt(1.0, 8.0, 4.0)
              == broke::PerformanceDeckOwner::Result::applied,
          "reviewed beat loop arms");
    check(scratch.begin() == broke::JogScratchController::Result::busy,
          "scratch refuses reviewed beat-loop cursor ownership");
    check(owner.beatLoopActive(), "rejected scratch preserves active beat loop");

    broke::PerformanceDeckOwner invalidOwner(engine, broke::deckCount);
    broke::JogScratchController invalid(engine, invalidOwner, broke::deckCount);
    check(invalid.begin() == broke::JogScratchController::Result::invalidDeck,
          "invalid deck fails closed");
}

void ownershipChangesDuringGestureFailClosed() {
    broke::Engine engine;
    engine.prepare(48000.0, 256);
    check(engine.submit(3, makeClip()), "mid-gesture ownership fixture submits");
    RenderBlock block;
    process(engine, block);

    auto& control = engine.control(3);
    control.rate.store(1.20f);
    control.playing.store(true);
    broke::PerformanceDeckOwner owner(engine, 3);
    broke::JogScratchController scratch(engine, owner, 3);

    check(scratch.begin() == broke::JogScratchController::Result::applied,
          "mid-gesture slip test begins from forward transport");
    check(owner.setSlipEnabled(true) == broke::PerformanceDeckOwner::Result::applied,
          "foreign slip owner can arrive after platter touch");
    const double seekBeforeSlipConflict = control.seek.load();
    check(scratch.setVelocity(-1.0) == broke::JogScratchController::Result::busy,
          "velocity update rejects slip ownership acquired mid-gesture");
    check(scratch.moveBySeconds(0.5) == broke::JogScratchController::Result::busy,
          "relative move rejects slip ownership acquired mid-gesture");
    check(owner.slipEnabled() && !owner.reverseEnabled()
              && control.seek.load() == seekBeforeSlipConflict,
          "rejected scratch update preserves foreign slip and seek state");
    check(scratch.end() == broke::JogScratchController::Result::busy,
          "release reports ownership conflict instead of resuming stale play state");
    check(!scratch.active() && owner.slipEnabled() && !owner.reverseEnabled()
              && !control.playing.load(),
          "conflicted release leaves foreign slip intact and transport stopped");
    check(std::abs(static_cast<double>(control.rate.load()) - 1.20) < 1.0e-6,
          "conflicted release restores pre-gesture rate without resuming playback");
    owner.clearReverseSlip();

    control.playing.store(true);
    check(scratch.begin() == broke::JogScratchController::Result::applied,
          "mid-gesture beat-loop test begins");
    broke::BeatGrid grid;
    check(grid.reset(0.0, 120.0), "mid-gesture beat-loop grid initializes");
    check(owner.setReviewedGrid(grid) == broke::PerformanceDeckOwner::Result::applied,
          "mid-gesture beat-loop grid accepted");
    check(owner.armBeatLoopAt(1.0, 8.0, 4.0)
              == broke::PerformanceDeckOwner::Result::applied,
          "beat loop can acquire transport after stationary platter touch");
    check(scratch.setVelocity(1.0) == broke::JogScratchController::Result::busy,
          "velocity update rejects beat-loop ownership acquired mid-gesture");
    check(scratch.end() == broke::JogScratchController::Result::busy,
          "release reports beat-loop ownership conflict");
    check(!scratch.active() && owner.beatLoopActive() && !control.playing.load(),
          "conflicted release preserves beat loop and does not resume old playback");
    owner.disarmLoop();
}

void forcedCancelAndDestructorStopTransport() {
    broke::Engine engine;
    engine.prepare(48000.0, 256);
    check(engine.submit(0, makeClip()), "forced-cancel fixture submits");
    RenderBlock block;
    process(engine, block);

    auto& control = engine.control(0);
    control.rate.store(1.25f);
    control.playing.store(true);
    broke::PerformanceDeckOwner owner(engine, 0);
    broke::JogScratchController scratch(engine, owner, 0);
    check(scratch.begin() == broke::JogScratchController::Result::applied,
          "forced-cancel gesture begins");
    check(scratch.setVelocity(-0.9) == broke::JogScratchController::Result::applied,
          "forced-cancel fixture enters reverse transport");
    check(scratch.cancel() == broke::JogScratchController::Result::applied,
          "forced cancel terminates active platter ownership");
    check(!scratch.active() && !control.playing.load() && !control.reverse.load(),
          "forced cancel stops playback and clears transient reverse");
    check(std::abs(static_cast<double>(control.rate.load()) - 1.25) < 1.0e-6,
          "forced cancel restores the user's pre-gesture rate");
    check(scratch.cancel() == broke::JogScratchController::Result::notActive,
          "double forced cancel is deterministic");

    control.rate.store(1.30f);
    control.playing.store(true);
    {
        broke::JogScratchController scoped(engine, owner, 0);
        check(scoped.begin() == broke::JogScratchController::Result::applied,
              "scoped gesture begins");
        check(scoped.setVelocity(-1.1) == broke::JogScratchController::Result::applied,
              "scoped gesture enters reverse transport");
    }
    check(!control.playing.load() && !control.reverse.load(),
          "controller destruction fail-closes active transport");
    check(std::abs(static_cast<double>(control.rate.load()) - 1.30) < 1.0e-6,
          "controller destruction preserves the user's pre-gesture rate");
}
} // namespace

int main() {
    try {
        signedVelocityDrivesRealProductionTransport();
        relativeMoveUsesAudibleCursorAndTrackBounds();
        conflictingOwnersFailClosed();
        ownershipChangesDuringGestureFailClosed();
        forcedCancelAndDestructorStopTransport();
        std::cout << "JogScratchControllerTests: " << checks << " checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "JogScratchControllerTests failed after " << checks
                  << " checks: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
