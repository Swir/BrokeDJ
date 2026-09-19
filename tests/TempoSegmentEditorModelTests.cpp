// SPDX-License-Identifier: AGPL-3.0-only
#include "core/TempoSegmentEditor.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
int checks = 0;

void check(bool condition, const char* name) {
    ++checks;
    if (!condition) throw std::runtime_error(name);
}

void checkClose(double actual, double expected, double tolerance, const char* name) {
    check(std::isfinite(actual) && std::abs(actual - expected) <= tolerance, name);
}

broke::BeatGrid variableGrid() {
    broke::BeatGrid grid;
    check(grid.reset(0.5, 120.0), "editor fixture base grid initializes");
    check(grid.insertTempoChangeAtBeat(8.0, 100.0), "editor fixture first boundary inserts");
    check(grid.insertTempoChangeAtBeat(16.0, 128.0), "editor fixture second boundary inserts");
    return grid;
}

void rowsExposeStableBeatAndTimeCoordinates() {
    broke::Engine engine;
    broke::PerformanceDeckOwner owner(engine, 0);
    check(owner.setReviewedGrid(variableGrid()) == broke::PerformanceDeckOwner::Result::applied,
          "editor owner accepts reviewed grid");

    broke::TempoSegmentEditorModel editor(owner);
    const auto rows = editor.segments();
    check(rows.size() == 3, "editor exposes all reviewed tempo segments");
    check(rows[0].base && !rows[1].base && !rows[2].base,
          "editor distinguishes immutable base boundary");
    checkClose(rows[0].beat, 0.0, 1.0e-12, "base boundary is beat zero");
    checkClose(rows[1].beat, 8.0, 1.0e-9, "first boundary preserves musical beat");
    checkClose(rows[2].beat, 16.0, 1.0e-9, "second boundary preserves musical beat");
    checkClose(rows[1].startSeconds, 4.5, 1.0e-9, "first boundary exposes source time");
    checkClose(rows[2].startSeconds, 9.3, 1.0e-9, "second boundary follows prior tempo");
}

void selectionFollowsCrossingMoveAndReplacement() {
    broke::Engine engine;
    broke::PerformanceDeckOwner owner(engine, 1);
    check(owner.setReviewedGrid(variableGrid()) == broke::PerformanceDeckOwner::Result::applied,
          "reorder owner accepts reviewed grid");

    broke::TempoSegmentEditorModel editor(owner);
    check(editor.select(1) == broke::PerformanceDeckOwner::Result::applied,
          "later tempo segment can be selected");
    check(editor.replaceSelected(12.0, 110.0) == broke::PerformanceDeckOwner::Result::applied,
          "selected boundary can move and change bpm transactionally");

    auto selected = editor.selectedSegment();
    check(selected.has_value(), "selection survives replacement");
    checkClose(selected->beat, 12.0, 1.0e-9, "selection follows replacement beat");
    checkClose(selected->bpm, 110.0, 1.0e-9, "replacement bpm is visible");

    check(editor.moveSelectedToBeat(20.0) == broke::PerformanceDeckOwner::Result::applied,
          "selected boundary can cross a neighboring segment");
    selected = editor.selectedSegment();
    check(selected.has_value() && selected->index == 2,
          "selection rebinds to moved boundary after ordering changes");
    checkClose(selected->beat, 20.0, 1.0e-9, "crossed boundary retains requested beat");

    check(editor.setSelectedBpm(111.0) == broke::PerformanceDeckOwner::Result::applied,
          "selected moved boundary accepts bpm edit");
    selected = editor.selectedSegment();
    check(selected.has_value(), "selection remains valid after bpm edit");
    checkClose(selected->bpm, 111.0, 1.0e-9, "selected bpm edit is committed");
}

void playheadTimeActionsUseCurrentReviewedMapping() {
    broke::Engine engine;
    broke::PerformanceDeckOwner owner(engine, 0);
    check(owner.setReviewedGrid(variableGrid()) == broke::PerformanceDeckOwner::Result::applied,
          "playhead owner accepts reviewed grid");

    broke::TempoSegmentEditorModel editor(owner);
    check(editor.select(1) == broke::PerformanceDeckOwner::Result::applied,
          "playhead move segment selection succeeds");

    const double beat12Time = owner.reviewedGrid().timeAtBeat(12.0);
    check(std::isfinite(beat12Time), "playhead fixture resolves beat 12 time");
    check(editor.moveSelectedToTime(beat12Time) == broke::PerformanceDeckOwner::Result::applied,
          "playhead time move converts through current reviewed map");
    auto selected = editor.selectedSegment();
    check(selected.has_value(), "playhead move keeps selection");
    checkClose(selected->beat, 12.0, 1.0e-9,
               "playhead move lands on the musical beat resolved before mutation");

    const double beat24Time = owner.reviewedGrid().timeAtBeat(24.0);
    check(std::isfinite(beat24Time), "playhead add fixture resolves beat 24 time after prior edit");
    check(editor.addAtTime(beat24Time, 96.0) == broke::PerformanceDeckOwner::Result::applied,
          "playhead time add uses the current reviewed mapping");
    selected = editor.selectedSegment();
    check(selected.has_value(), "new playhead boundary becomes selected");
    checkClose(selected->beat, 24.0, 1.0e-9, "playhead add preserves resolved musical beat");
    checkClose(selected->bpm, 96.0, 1.0e-9, "playhead add preserves requested BPM");

    const auto before = owner.reviewedGrid();
    const auto countBefore = editor.segmentCount();
    check(editor.addAtTime(-1.0, 120.0) == broke::PerformanceDeckOwner::Result::invalidRequest,
          "negative playhead add fails closed");
    check(editor.addAtTime(std::numeric_limits<double>::quiet_NaN(), 120.0)
              == broke::PerformanceDeckOwner::Result::invalidRequest,
          "non-finite playhead add fails closed");
    check(editor.segmentCount() == countBefore,
          "invalid playhead actions preserve complete map size");
    checkClose(owner.reviewedGrid().timeAtBeat(32.0), before.timeAtBeat(32.0), 1.0e-9,
               "invalid playhead actions preserve reviewed mapping");
}

void addRemoveAndInvalidRequestsFailClosed() {
    broke::Engine engine;
    broke::PerformanceDeckOwner owner(engine, 2);
    check(owner.setReviewedGrid(variableGrid()) == broke::PerformanceDeckOwner::Result::applied,
          "add/remove owner accepts reviewed grid");

    broke::TempoSegmentEditorModel editor(owner);
    const double beat24Time = owner.reviewedGrid().timeAtBeat(24.0);
    check(editor.addAtTime(beat24Time, 90.0) == broke::PerformanceDeckOwner::Result::applied,
          "editor inserts tempo boundary from source time");
    check(editor.segmentCount() == 4, "successful add grows reviewed map");

    auto selected = editor.selectedSegment();
    check(selected.has_value(), "newly added boundary becomes selected");
    checkClose(selected->beat, 24.0, 1.0e-9, "added boundary selection resolves exact beat");
    checkClose(selected->bpm, 90.0, 1.0e-9, "added boundary stores requested bpm");

    const auto preservedGrid = owner.reviewedGrid();
    const auto preservedSelection = editor.selectedIndex();
    check(editor.addAtBeat(24.0, 140.0) == broke::PerformanceDeckOwner::Result::invalidRequest,
          "duplicate boundary insertion is rejected");
    check(owner.reviewedGrid().segments().size() == preservedGrid.segments().size(),
          "rejected add preserves complete reviewed map");
    check(editor.selectedIndex() == preservedSelection,
          "rejected add preserves current selection");

    check(editor.removeSelected() == broke::PerformanceDeckOwner::Result::applied,
          "selected later boundary can be removed");
    check(editor.segmentCount() == 3, "remove shrinks reviewed map");
    selected = editor.selectedSegment();
    check(selected.has_value() && selected->index == 2,
          "remove selects nearest surviving previous row");

    check(editor.select(0) == broke::PerformanceDeckOwner::Result::applied,
          "base segment can be selected for bpm editing");
    check(editor.moveSelectedToBeat(4.0) == broke::PerformanceDeckOwner::Result::invalidRequest,
          "base boundary cannot be moved");
    check(editor.removeSelected() == broke::PerformanceDeckOwner::Result::invalidRequest,
          "base boundary cannot be removed");
    check(editor.setSelectedBpm(121.0) == broke::PerformanceDeckOwner::Result::applied,
          "base segment bpm remains editable");
    checkClose(owner.reviewedGrid().segments().front().bpm, 121.0, 1.0e-9,
               "base bpm edit reaches reviewed owner");
}

void staleSelectionCannotEditReplacementGrid() {
    broke::Engine engine;
    broke::PerformanceDeckOwner owner(engine, 3);
    check(owner.setReviewedGrid(variableGrid()) == broke::PerformanceDeckOwner::Result::applied,
          "stale-selection owner accepts initial grid");

    broke::TempoSegmentEditorModel editor(owner);
    check(editor.select(1) == broke::PerformanceDeckOwner::Result::applied,
          "initial segment selection succeeds");

    broke::BeatGrid replacement;
    check(replacement.reset(1.0, 130.0), "replacement grid initializes");
    check(replacement.insertTempoChangeAtBeat(8.0, 105.0),
          "replacement grid has same row count with different identity");
    check(replacement.insertTempoChangeAtBeat(16.0, 125.0),
          "replacement grid second boundary inserts");
    check(owner.setReviewedGrid(replacement) == broke::PerformanceDeckOwner::Result::applied,
          "owner replaces reviewed grid externally");

    check(!editor.selectedIndex().has_value(),
          "selection anchor invalidates after reviewed grid replacement");
    const auto before = owner.reviewedGrid().segments()[1].bpm;
    check(editor.setSelectedBpm(99.0) == broke::PerformanceDeckOwner::Result::invalidRequest,
          "stale selection fails closed instead of editing replacement grid");
    checkClose(owner.reviewedGrid().segments()[1].bpm, before, 1.0e-12,
               "failed stale edit preserves replacement grid");

    owner.resetForClip();
    check(editor.addAtBeat(8.0, 120.0) == broke::PerformanceDeckOwner::Result::gridUnavailable,
          "clip reset leaves editor safely unavailable");
    check(editor.segments().empty(), "clip reset exposes no stale segment rows");
}

} // namespace

int main() {
    try {
        rowsExposeStableBeatAndTimeCoordinates();
        selectionFollowsCrossingMoveAndReplacement();
        playheadTimeActionsUseCurrentReviewedMapping();
        addRemoveAndInvalidRequestsFailClosed();
        staleSelectionCannotEditReplacementGrid();
        std::cout << "Tempo segment editor model checks passed: " << checks << "\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Tempo segment editor model test failed after " << checks
                  << " checks: " << error.what() << "\n";
        return EXIT_FAILURE;
    }
}