// SPDX-License-Identifier: AGPL-3.0-only
#include "core/PerformanceDeckOwner.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
int checks = 0;

void check(bool condition, const char* name) {
    ++checks;
    if (!condition) throw std::runtime_error(name);
}

void reviewedBeatLoopOwnershipIsTransactional() {
    broke::Engine engine;
    broke::PerformanceDeckOwner owner(engine, 0);

    broke::BeatGrid grid;
    check(grid.reset(0.5, 120.0), "reviewed loop grid initializes");
    check(grid.insertTempoChangeAtBeat(8.0, 90.0),
          "reviewed loop grid contains a tempo change");
    check(owner.setReviewedGrid(grid) == broke::PerformanceDeckOwner::Result::applied,
          "valid reviewed grid is accepted");

    check(owner.armBeatLoopAt(3.7, 20.0, 4.0)
              == broke::PerformanceDeckOwner::Result::applied,
          "four-beat loop arms from reviewed grid");
    check(owner.beatLoopActive(), "owner reports armed beat loop");
    check(std::abs(owner.beatLoopLength() - 4.0) < 1.0e-12,
          "owner preserves requested musical loop length");
    check(engine.loopRegionEnabled(0), "engine custom loop region is armed");
    check(engine.control(0).loop.load(), "engine loop switch is enabled only after region publish");

    const auto tailFailure = owner.armBeatLoopAt(19.8, 20.0, 8.0);
    check(tailFailure == broke::PerformanceDeckOwner::Result::outsideTrack,
          "loop extending beyond track fails closed");
    check(owner.beatLoopActive() && std::abs(owner.beatLoopLength() - 4.0) < 1.0e-12,
          "failed re-arm preserves previous valid loop transactionally");

    broke::BeatGrid corrected;
    check(corrected.reset(0.6, 122.0), "replacement reviewed grid initializes");
    check(owner.setReviewedGrid(corrected) == broke::PerformanceDeckOwner::Result::applied,
          "replacement reviewed grid is accepted");
    check(!owner.beatLoopActive() && !engine.loopRegionEnabled(0)
              && !engine.control(0).loop.load(),
          "grid replacement disarms stale beat-derived region");

    check(owner.setWholeTrackLoop(true) == broke::PerformanceDeckOwner::Result::applied,
          "whole-track loop remains an explicit independent mode");
    check(engine.control(0).loop.load() && !engine.loopRegionEnabled(0),
          "whole-track loop does not masquerade as a beat region");
    check(owner.setWholeTrackLoop(false) == broke::PerformanceDeckOwner::Result::applied,
          "whole-track loop can be disabled without a reviewed grid");
}

void hotCuesUseReviewedGridAndSafeTransportRules() {
    broke::Engine engine;
    broke::PerformanceDeckOwner owner(engine, 1);
    broke::BeatGrid grid;
    check(grid.reset(0.5, 120.0), "hotcue grid initializes");
    check(owner.setReviewedGrid(grid) == broke::PerformanceDeckOwner::Result::applied,
          "hotcue owner accepts reviewed grid");

    check(owner.storeHotCueAt(0, 1.30, 20.0, true)
              == broke::PerformanceDeckOwner::Result::applied,
          "quantized hotcue stores successfully");
    const auto& cue = owner.hotCue(0);
    check(cue.set && cue.quantized && std::abs(cue.seconds - 1.5) < 1.0e-9,
          "quantized hotcue resolves nearest reviewed beat");

    check(owner.setWholeTrackLoop(true) == broke::PerformanceDeckOwner::Result::applied,
          "whole-track loop fixture enables");
    check(owner.triggerHotCue(0, 20.0) == broke::PerformanceDeckOwner::Result::applied,
          "stored hotcue triggers normalized seek");
    check(std::abs(engine.control(1).seek.load() - 0.075) < 1.0e-12,
          "hotcue publishes exact normalized transport target");
    check(engine.control(1).loop.load(),
          "hotcue trigger intentionally preserves whole-track loop mode");

    check(owner.armBeatLoopAt(3.7, 20.0, 4.0)
              == broke::PerformanceDeckOwner::Result::applied,
          "beat-loop fixture arms before cue jump");
    check(owner.triggerHotCue(0, 20.0) == broke::PerformanceDeckOwner::Result::applied,
          "hotcue triggers while beat loop is active");
    check(!owner.beatLoopActive() && !engine.loopRegionEnabled(1)
              && !engine.control(1).loop.load(),
          "cue jump exits stale beat loop rather than reinterpreting new cursor");

    owner.clearReviewedGrid();
    check(owner.storeHotCueAt(1, 2.25, 20.0, true)
              == broke::PerformanceDeckOwner::Result::gridUnavailable,
          "quantized hotcue refuses missing reviewed grid");
    check(owner.storeHotCueAt(1, 2.25, 20.0, false)
              == broke::PerformanceDeckOwner::Result::applied,
          "explicit unquantized hotcue remains available without grid");
    check(std::abs(owner.hotCue(1).seconds - 2.25) < 1.0e-12,
          "unquantized cue preserves requested source time");
}

class BusyRenderer final : public broke::DeckSourceRenderer {
public:
    bool render(const broke::Clip&, double, bool, double,
                float*, float*, int, double&, double&) noexcept override {
        return false;
    }
};

void failClosedBoundariesDoNotDriftControls() {
    broke::Engine engine;
    broke::PerformanceDeckOwner owner(engine, 2);
    broke::BeatGrid grid;
    check(grid.reset(0.0, 128.0), "busy-renderer grid initializes");
    check(owner.setReviewedGrid(grid) == broke::PerformanceDeckOwner::Result::applied,
          "busy-renderer owner accepts reviewed grid");

    BusyRenderer renderer;
    check(engine.setDeckSourceRenderer(2, &renderer), "external renderer takes deck ownership while stopped");
    engine.control(2).loop.store(false);
    check(owner.armBeatLoopAt(2.0, 30.0, 4.0)
              == broke::PerformanceDeckOwner::Result::rendererBusy,
          "beat loop refuses deck owned by another source renderer");
    check(!engine.control(2).loop.load() && !owner.beatLoopActive(),
          "renderer refusal does not drift loop control state");

    broke::PerformanceDeckOwner invalid(engine, broke::deckCount);
    check(invalid.setWholeTrackLoop(true) == broke::PerformanceDeckOwner::Result::invalidDeck,
          "invalid deck owner fails closed");
    check(invalid.storeHotCueAt(0, 1.0, 10.0, false)
              == broke::PerformanceDeckOwner::Result::invalidDeck,
          "invalid deck cannot store cues");

    broke::Engine resetEngine;
    broke::PerformanceDeckOwner resetOwner(resetEngine, 0);
    check(resetOwner.setReviewedGrid(grid) == broke::PerformanceDeckOwner::Result::applied,
          "reset fixture accepts reviewed grid");
    check(resetOwner.storeHotCueAt(0, 1.0, 10.0, true)
              == broke::PerformanceDeckOwner::Result::applied,
          "reset fixture stores cue");
    check(resetOwner.armBeatLoopAt(2.0, 10.0, 4.0)
              == broke::PerformanceDeckOwner::Result::applied,
          "reset fixture arms beat loop");
    resetOwner.resetForClip();
    check(!resetOwner.hasReviewedGrid() && !resetOwner.hotCue(0).set
              && !resetOwner.beatLoopActive() && !resetEngine.loopRegionEnabled(0)
              && !resetEngine.control(0).loop.load()
              && resetEngine.control(0).seek.load() < 0.0,
          "clip replacement clears all source-identity-bound performance state");
}
} // namespace

int main() {
    try {
        reviewedBeatLoopOwnershipIsTransactional();
        hotCuesUseReviewedGridAndSafeTransportRules();
        failClosedBoundariesDoNotDriftControls();
        std::cout << "PerformanceDeckOwnerTests: " << checks << " checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "PerformanceDeckOwnerTests failed after " << checks
                  << " checks: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
