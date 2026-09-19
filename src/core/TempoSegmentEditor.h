// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "PerformanceDeckOwner.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

namespace broke {

// Message/control-thread model for the compact variable-tempo editor.
//
// The model deliberately sits above PerformanceDeckOwner instead of mutating a
// BeatGrid directly. That keeps all accepted edits behind the same reviewed-grid
// ownership boundary used by Beat Loop / Hot Cue / Beat Jump / Sync. It performs
// no disk I/O and is never called by Engine::process(); persistence remains an
// application-adapter responsibility after a successful edit.
//
// A selection is anchored to the boundary values observed when it was selected.
// If another owner operation or a track replacement changes that boundary, the
// next mutation fails closed until the UI refreshes/reselects. This prevents a
// stale ComboBox/list row from editing an unrelated tempo segment.
//
// A bounded message-thread undo/redo history stores complete reviewed-grid
// snapshots. History is identity-checked against the owner's current grid before
// use, so an external track/grid replacement can never restore data from a stale
// clip. Applying undo/redo still routes through PerformanceDeckOwner and therefore
// preserves the same loop invalidation and validation boundary as ordinary edits.
class TempoSegmentEditorModel final {
public:
    using Result = PerformanceDeckOwner::Result;
    static constexpr std::size_t historyLimit = 16;

    struct SegmentDescriptor final {
        std::size_t index = 0;
        double startSeconds = 0.0;
        double beat = 0.0;
        double bpm = 0.0;
        bool base = false;
    };

    explicit TempoSegmentEditorModel(PerformanceDeckOwner& target) noexcept
        : owner(target) {}

    [[nodiscard]] bool available() const noexcept {
        return owner.hasReviewedGrid();
    }

    [[nodiscard]] std::size_t segmentCount() const noexcept {
        return available() ? owner.reviewedGrid().segments().size() : 0;
    }

    [[nodiscard]] std::vector<SegmentDescriptor> segments() const {
        std::vector<SegmentDescriptor> result;
        if (!available()) return result;

        const auto& grid = owner.reviewedGrid();
        const auto& source = grid.segments();
        result.reserve(source.size());
        for (std::size_t index = 0; index < source.size(); ++index) {
            const double beat = grid.tempoChangeBeat(index);
            if (!std::isfinite(beat)) {
                result.clear();
                return result;
            }
            result.push_back({
                index,
                source[index].startSeconds,
                beat,
                source[index].bpm,
                index == 0
            });
        }
        return result;
    }

    [[nodiscard]] std::optional<std::size_t> selectedIndex() const noexcept {
        return selectionMatchesCurrent()
            ? std::optional<std::size_t>{selection->index}
            : std::nullopt;
    }

    [[nodiscard]] std::optional<SegmentDescriptor> selectedSegment() const noexcept {
        if (!selectionMatchesCurrent()) return std::nullopt;
        const auto& grid = owner.reviewedGrid();
        const auto& segment = grid.segments()[selection->index];
        return SegmentDescriptor{
            selection->index,
            segment.startSeconds,
            grid.tempoChangeBeat(selection->index),
            segment.bpm,
            selection->index == 0
        };
    }

    [[nodiscard]] bool canUndo() const noexcept {
        return available() && historyCurrent.has_value()
            && sameGrid(*historyCurrent, owner.reviewedGrid())
            && !undoHistory.empty();
    }

    [[nodiscard]] bool canRedo() const noexcept {
        return available() && historyCurrent.has_value()
            && sameGrid(*historyCurrent, owner.reviewedGrid())
            && !redoHistory.empty();
    }

    [[nodiscard]] Result select(std::size_t index) noexcept {
        if (!available()) {
            selection.reset();
            return Result::gridUnavailable;
        }

        const auto& grid = owner.reviewedGrid();
        if (index >= grid.segments().size()) {
            selection.reset();
            return Result::invalidRequest;
        }

        const double beat = grid.tempoChangeBeat(index);
        const auto& segment = grid.segments()[index];
        if (!std::isfinite(beat)
            || !std::isfinite(segment.startSeconds)
            || !std::isfinite(segment.bpm)) {
            selection.reset();
            return Result::invalidRequest;
        }

        selection = SelectionAnchor{
            index,
            segment.startSeconds,
            beat,
            segment.bpm
        };
        return Result::applied;
    }

    [[nodiscard]] Result selectAtTime(double seconds) noexcept {
        if (!available()) {
            selection.reset();
            return Result::gridUnavailable;
        }
        if (!std::isfinite(seconds) || seconds < 0.0) return Result::invalidRequest;

        const auto& source = owner.reviewedGrid().segments();
        if (source.empty()) {
            selection.reset();
            return Result::gridUnavailable;
        }

        std::size_t index = 0;
        for (std::size_t candidate = 1; candidate < source.size(); ++candidate) {
            if (seconds + boundaryTolerance < source[candidate].startSeconds) break;
            index = candidate;
        }
        return select(index);
    }

    void clearSelection() noexcept { selection.reset(); }

    [[nodiscard]] Result addAtBeat(double beat, double bpm) {
        if (!available()) return Result::gridUnavailable;
        if (!std::isfinite(beat) || !std::isfinite(bpm))
            return Result::invalidRequest;

        reconcileHistory();
        const auto before = owner.reviewedGrid();
        const auto result = owner.insertReviewedTempoChange(beat, bpm);
        if (result == Result::applied) {
            recordAcceptedMutation(before);
            selectBoundaryAtBeat(beat);
        }
        return result;
    }

    [[nodiscard]] Result addAtTime(double seconds, double bpm) {
        if (!available()) return Result::gridUnavailable;
        if (!std::isfinite(seconds) || seconds < 0.0 || !std::isfinite(bpm))
            return Result::invalidRequest;

        const double beat = owner.reviewedGrid().beatAtTime(seconds);
        if (!std::isfinite(beat)) return Result::invalidRequest;
        return addAtBeat(beat, bpm);
    }

    [[nodiscard]] Result setSelectedBpm(double bpm) {
        reconcileHistory();
        const auto index = validatedSelectionIndex();
        if (!index) return available() ? Result::invalidRequest : Result::gridUnavailable;
        if (!std::isfinite(bpm)) return Result::invalidRequest;

        const auto before = owner.reviewedGrid();
        const auto result = owner.setReviewedSegmentBpm(*index, bpm);
        if (result == Result::applied) {
            recordAcceptedMutation(before);
            static_cast<void>(select(*index));
        }
        return result;
    }

    [[nodiscard]] Result moveSelectedToBeat(double beat) {
        reconcileHistory();
        const auto index = validatedSelectionIndex();
        if (!index) return available() ? Result::invalidRequest : Result::gridUnavailable;
        if (*index == 0 || !std::isfinite(beat)) return Result::invalidRequest;

        const auto before = owner.reviewedGrid();
        const auto result = owner.moveReviewedTempoChange(*index, beat);
        if (result == Result::applied) {
            recordAcceptedMutation(before);
            selectBoundaryAtBeat(beat);
        }
        return result;
    }

    [[nodiscard]] Result moveSelectedToTime(double seconds) {
        const auto index = validatedSelectionIndex();
        if (!index) return available() ? Result::invalidRequest : Result::gridUnavailable;
        if (*index == 0 || !std::isfinite(seconds) || seconds < 0.0)
            return Result::invalidRequest;

        const double beat = owner.reviewedGrid().beatAtTime(seconds);
        if (!std::isfinite(beat)) return Result::invalidRequest;
        return moveSelectedToBeat(beat);
    }

    [[nodiscard]] Result replaceSelected(double beat, double bpm) {
        reconcileHistory();
        const auto index = validatedSelectionIndex();
        if (!index) return available() ? Result::invalidRequest : Result::gridUnavailable;
        if (*index == 0 || !std::isfinite(beat) || !std::isfinite(bpm))
            return Result::invalidRequest;

        const auto before = owner.reviewedGrid();
        const auto result = owner.replaceReviewedTempoChange(*index, beat, bpm);
        if (result == Result::applied) {
            recordAcceptedMutation(before);
            selectBoundaryAtBeat(beat);
        }
        return result;
    }

    [[nodiscard]] Result removeSelected() {
        reconcileHistory();
        const auto index = validatedSelectionIndex();
        if (!index) return available() ? Result::invalidRequest : Result::gridUnavailable;
        if (*index == 0) return Result::invalidRequest;

        const auto before = owner.reviewedGrid();
        const std::size_t removedIndex = *index;
        const auto result = owner.removeReviewedTempoChange(removedIndex);
        if (result != Result::applied) return result;

        recordAcceptedMutation(before);
        const std::size_t count = segmentCount();
        if (count == 0) {
            selection.reset();
            return result;
        }
        const std::size_t next = std::min(removedIndex - 1, count - 1);
        static_cast<void>(select(next));
        return result;
    }

    [[nodiscard]] Result undo() {
        reconcileHistory();
        if (!available()) return Result::gridUnavailable;
        if (undoHistory.empty()) return Result::invalidRequest;

        const auto current = owner.reviewedGrid();
        const auto target = undoHistory.back();
        const auto result = owner.setReviewedGrid(target);
        if (result != Result::applied) return result;

        undoHistory.pop_back();
        pushBounded(redoHistory, current);
        historyCurrent = owner.reviewedGrid();
        selection.reset();
        return result;
    }

    [[nodiscard]] Result redo() {
        reconcileHistory();
        if (!available()) return Result::gridUnavailable;
        if (redoHistory.empty()) return Result::invalidRequest;

        const auto current = owner.reviewedGrid();
        const auto target = redoHistory.back();
        const auto result = owner.setReviewedGrid(target);
        if (result != Result::applied) return result;

        redoHistory.pop_back();
        pushBounded(undoHistory, current);
        historyCurrent = owner.reviewedGrid();
        selection.reset();
        return result;
    }

private:
    struct SelectionAnchor final {
        std::size_t index = 0;
        double startSeconds = 0.0;
        double beat = 0.0;
        double bpm = 0.0;
    };

    static constexpr double boundaryTolerance = 1.0e-8;
    static constexpr double bpmTolerance = 1.0e-9;

    [[nodiscard]] static bool sameGrid(const BeatGrid& left, const BeatGrid& right) noexcept {
        if (left.valid() != right.valid()) return false;
        if (!left.valid()) return true;
        if (std::abs(left.beatZeroSeconds() - right.beatZeroSeconds()) > boundaryTolerance)
            return false;
        const auto& a = left.segments();
        const auto& b = right.segments();
        if (a.size() != b.size()) return false;
        for (std::size_t index = 0; index < a.size(); ++index) {
            if (std::abs(a[index].startSeconds - b[index].startSeconds) > boundaryTolerance
                || std::abs(a[index].bpm - b[index].bpm) > bpmTolerance) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool selectionMatchesCurrent() const noexcept {
        if (!selection || !available()) return false;
        const auto& grid = owner.reviewedGrid();
        if (selection->index >= grid.segments().size()) return false;

        const auto& segment = grid.segments()[selection->index];
        const double beat = grid.tempoChangeBeat(selection->index);
        return std::isfinite(beat)
            && std::abs(segment.startSeconds - selection->startSeconds) <= boundaryTolerance
            && std::abs(beat - selection->beat) <= boundaryTolerance
            && std::abs(segment.bpm - selection->bpm) <= bpmTolerance;
    }

    [[nodiscard]] std::optional<std::size_t> validatedSelectionIndex() const noexcept {
        return selectionMatchesCurrent()
            ? std::optional<std::size_t>{selection->index}
            : std::nullopt;
    }

    void selectBoundaryAtBeat(double beat) noexcept {
        if (!available() || !std::isfinite(beat)) {
            selection.reset();
            return;
        }

        const auto& grid = owner.reviewedGrid();
        for (std::size_t index = 0; index < grid.segments().size(); ++index) {
            const double candidate = grid.tempoChangeBeat(index);
            if (std::isfinite(candidate)
                && std::abs(candidate - beat) <= boundaryTolerance) {
                static_cast<void>(select(index));
                return;
            }
        }
        selection.reset();
    }

    static void pushBounded(std::vector<BeatGrid>& history, const BeatGrid& grid) {
        if (history.size() >= historyLimit) history.erase(history.begin());
        history.push_back(grid);
    }

    void clearHistoryInternal() noexcept {
        undoHistory.clear();
        redoHistory.clear();
        historyCurrent.reset();
    }

    void reconcileHistory() {
        if (!available()) {
            clearHistoryInternal();
            return;
        }
        if (historyCurrent && !sameGrid(*historyCurrent, owner.reviewedGrid()))
            clearHistoryInternal();
    }

    void recordAcceptedMutation(const BeatGrid& before) {
        const auto& current = owner.reviewedGrid();
        if (!sameGrid(before, current)) {
            pushBounded(undoHistory, before);
            redoHistory.clear();
        }
        historyCurrent = current;
    }

    PerformanceDeckOwner& owner;
    std::optional<SelectionAnchor> selection;
    std::vector<BeatGrid> undoHistory;
    std::vector<BeatGrid> redoHistory;
    std::optional<BeatGrid> historyCurrent;
};

} // namespace broke
