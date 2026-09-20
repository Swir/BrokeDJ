# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness or live-performance claim.

## Current checkpoint

- Verified default-branch baseline: `main` at `5c41243ab0a8fa68b76ae6fd0559a7da965c0d3e` (`M2: harden variable-tempo segment edit ownership`). Merged-main GitHub Actions run `35470560672` passed.
- Active development branch: `feat/m2-tempo-segment-editor-model`, PR #38. The last verified PR head `8521c8d78b6c2027481405a865265cddd4e7b270` passed exact-head run `35473162932`; the current checkpoint extends that branch and requires a fresh exact-head run before merge.
- Active package: native variable-tempo map editing plus fail-closed playhead navigation and bounded per-track Undo/Redo. Every accepted mutation still routes through `TempoSegmentEditorModel -> PerformanceDeckOwner` and the complete reviewed grid is persisted asynchronously through `TrackBeatGridOverrideStore`.
- Undo/Redo is intentionally bounded to 16 complete reviewed-grid snapshots and is invalidated automatically when the owner grid changes outside the editor, preventing history from restoring data from a replaced track/grid.
- No persistence, decoding, network/file I/O or unbounded work is added to `Engine::process()`.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA).
- No public BrokeDJ Release exists or is qualified by this checkpoint.

## Newly integrated on main: variable-tempo edit ownership

1. **Transactional reviewed-grid primitives**
   - `BeatGrid` can insert, move, replace and remove later tempo boundaries while preserving continuous musical beat numbering.
   - Segment zero remains the immutable beat-zero boundary; invalid/duplicate edits fail without partially mutating the reviewed map.

2. **Performance-owner mutation boundary**
   - `PerformanceDeckOwner` owns reviewed segment BPM edits and tempo-boundary insert/move/replace/remove operations.
   - Successful map replacement invalidates any armed beat-derived loop whose source-time bounds came from the old map; rejected edits preserve the previous grid and loop state.
   - These operations are message/control-thread work and add no file/network I/O, decoding, locks or allocation work to `Engine::process()`.

3. **Verified merged baseline**
   - GitHub Actions run `35470560672` passed for exact `main` head `5c41243ab0a8fa68b76ae6fd0559a7da965c0d3e`.
   - This automated baseline does not close M1 hardware validation or M2 music-domain/listening gates.

## Current development package: native tempo-map editor

1. **Compact deck workflow**
   - A reviewed-grid deck exposes a `Tempo map` action without replacing the compact base-grid controls.
   - The dialog lists every tempo segment with beat and BPM, permits BPM editing for the immutable base segment, and exposes add/move/remove/replace actions for later boundaries.
   - `ADD @ PLAYHEAD` and `MOVE @ PLAYHEAD` convert the current source-time cursor through the current reviewed map before mutation; invalid/unavailable transport fails closed.

2. **Playhead navigation and recoverable editing**
   - `SELECT @ PLAYHEAD` selects the segment currently governing the source-time playhead without changing the map, making long variable-tempo maps faster to navigate than row-only selection.
   - The editor keeps a bounded 16-step history of complete reviewed-grid snapshots. Undo/Redo applies through `PerformanceDeckOwner`, so validation and beat-loop invalidation remain identical to normal edits.
   - History is bound to the exact current reviewed-grid state. Track replacement, detector reset or any external grid replacement invalidates stale history before it can mutate the owner.

3. **Owner-bound safety and lifecycle**
   - All native actions use the tested `TempoSegmentEditorModel`, which in turn mutates only through `PerformanceDeckOwner`.
   - Stale row selections cannot edit a replaced grid. Loading another track or resetting to detector output closes the matching tempo editor rather than leaving stale controls attached to the old clip.
   - A successful reviewed-map replacement deliberately disarms any beat-derived loop calculated from the previous map; ordinary whole-track loop semantics remain separate.

4. **Asynchronous complete-map persistence**
   - Accepted owner snapshots are copied into the deck's reviewed-grid state immediately on the message thread, then the complete valid map is queued to the existing analysis/persistence worker.
   - `TrackBeatGridOverrideStore` keeps source-identity binding and omits the raw local path/name from its payload. Persistence failure leaves the accepted session map active but reports the failure explicitly.
   - Undo and Redo use the same accepted-edit callback, so the restored complete map is queued through the same persistence path instead of becoming a session-only hidden state.

5. **Expanded deterministic model coverage**
   - `tempo_segment_editor_model` exercises playhead-time add/move behavior, playhead segment selection, complete-map undo/redo, external-grid history invalidation, stale-selection rejection and negative/non-finite preservation.
   - The target remains JUCE-independent and participates in Linux sanitizer/core and Windows CTest validation.

## Gates still open

- Fresh exact-head Linux/Windows CI for the extended PR #38; do not merge until the final head is green.
- Native manual interaction review of the dialog: four-deck resize/HiDPI, rapid track replacement while the editor is open, repeated Undo/Redo plus persistence/reload on a real Windows 11 machine.
- Real Windows 11 clean-machine/audio-interface validation, including device switching and independent four-output cue where supported.
- Representative legal local-music corpus validation for BPM/key/grid behavior, especially genuinely variable-tempo material.
- Real listening/soak, controller and production-release qualification.
- Production key lock, slip/reverse/scratch and broader M2 workflow completion.

## Next largest step

Require a green exact-head run for the current PR #38 head and fix any regression before integration. Continue on the same branch/PR during the normal integration window: first close any tempo-map persistence/navigation defects exposed by CI, then move into the next independent M2 performance-deck gap, prioritizing reverse/slip transport semantics and deterministic realtime tests over cosmetic feature growth.
