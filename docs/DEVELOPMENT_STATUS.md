# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness or live-performance claim.

## Current checkpoint

- Verified default-branch baseline: `main` at `5c41243ab0a8fa68b76ae6fd0559a7da965c0d3e` (`M2: harden variable-tempo segment edit ownership`). Merged-main GitHub Actions run `35470560672` passed.
- Active development branch: `feat/m2-tempo-segment-editor-model`, PR #38. The previous PR head `97444fbda1f08ba886e11eb9dca5ca57b0d0328f` passed exact-head run `35471308795`; this checkpoint extends that PR and therefore requires a new exact-head run before merge.
- Active package: native variable-tempo map editing on top of the JUCE-independent fail-closed editor model. Each deck gains a compact `Tempo map` dialog for segment selection, add-at-playhead, exact beat/BPM replacement, move-to-playhead and removal of later boundaries.
- Accepted edits remain behind `TempoSegmentEditorModel -> PerformanceDeckOwner`; the full reviewed grid is mirrored into deck UI state and persisted asynchronously through `TrackBeatGridOverrideStore`. No persistence, decoding or unbounded work is added to `Engine::process()`.
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
   - `ADD @ PLAYHEAD` and `MOVE @ PLAYHEAD` convert the current source-time cursor through the current reviewed map before mutation; out-of-track or unavailable transport fails closed.

2. **Owner-bound safety and lifecycle**
   - All native actions use the tested `TempoSegmentEditorModel`, which in turn mutates only through `PerformanceDeckOwner`.
   - Stale row selections cannot edit a replaced grid. Loading another track or resetting to detector output closes the matching tempo editor rather than leaving stale controls attached to the old clip.
   - A successful reviewed-map replacement deliberately disarms any beat-derived loop calculated from the previous map; ordinary whole-track loop semantics remain separate.

3. **Asynchronous complete-map persistence**
   - Accepted owner snapshots are copied into the deck's reviewed-grid state immediately on the message thread, then the complete valid map is queued to the existing analysis/persistence worker.
   - `TrackBeatGridOverrideStore` keeps source-identity binding and omits the raw local path/name from its payload. Persistence failure leaves the accepted session map active but reports the failure explicitly.
   - Existing override-store tests already cover multi-segment round-trip, source-identity invalidation, privacy of payload and rejected invalid writes.

4. **Expanded deterministic model coverage**
   - `tempo_segment_editor_model` now additionally exercises playhead-time add/move behavior against the current variable-tempo mapping, plus negative/non-finite input preservation.
   - The target remains JUCE-independent and participates in Linux sanitizer/core and Windows CTest validation.

## Gates still open

- New exact-head Linux/Windows CI for the extended PR #38; do not merge until the final head is green.
- Native manual interaction review of the new dialog: four-deck resize/HiDPI, rapid track replacement while the editor is open, and persistence/reload on a real Windows 11 machine.
- Real Windows 11 clean-machine/audio-interface validation, including device switching and independent four-output cue where supported.
- Representative legal local-music corpus validation for BPM/key/grid behavior, especially genuinely variable-tempo material.
- Real listening/soak, controller and production-release qualification.
- Production key lock, slip/reverse/scratch and broader M2 workflow completion.

## Next largest step

First require a green exact-head run for the extended PR #38 and fix any regression before integration. After the native tempo-map workflow is stable, run it against representative variable-tempo tracks and harden edit/reload/session behavior; then continue the largest independent M2 gap that does not depend on unavailable hardware, with slip/reverse transport semantics ahead of cosmetic feature growth.