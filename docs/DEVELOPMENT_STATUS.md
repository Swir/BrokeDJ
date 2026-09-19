# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness or live-performance claim.

## Current checkpoint

- Verified default-branch baseline: `main` at `5c41243ab0a8fa68b76ae6fd0559a7da965c0d3e` (`M2: harden variable-tempo segment edit ownership`). Merged-main GitHub Actions run `35470560672` passed.
- Active development branch: `feat/m2-tempo-segment-editor-model`.
- Active package: a JUCE-independent, owner-bound variable-tempo segment editor model plus deterministic CTest coverage. It is a safety/interaction layer for the next native DeckPanel segment editor; it is not yet the user-facing add/move/remove control surface.
- Exact-head CI for this development checkpoint is pending and must pass before any merge to `main`.
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

## Current development package: tempo-segment editor model

1. **Owner-bound editor state**
   - `TempoSegmentEditorModel` exposes validated segment rows in both source-time and musical-beat coordinates and routes every mutation through `PerformanceDeckOwner` rather than editing a `BeatGrid` behind the owner boundary.
   - The model supports selecting a row, adding a boundary by beat or source time, moving/replacing the selected boundary, BPM editing and removal.
   - The base boundary can be selected for BPM editing but cannot be moved or removed.

2. **Stale-selection fail-closed behavior**
   - Selection is anchored to the boundary values observed when the row was selected.
   - If a track/grid replacement changes that boundary, the next mutation refuses the stale selection until the UI refreshes/reselects, preventing a stale row index from editing an unrelated tempo segment.
   - When a selected boundary legitimately crosses another boundary, the model rebinds selection by musical beat instead of trusting the old numeric row index.

3. **Deterministic regression target**
   - New `tempo_segment_editor_model` CTest coverage checks row coordinates, crossing moves, combined beat/BPM replacement, add/remove behavior, duplicate rejection, immutable base-boundary rules, stale-selection rejection and clip-reset unavailability.
   - The target links only the JUCE-independent core and therefore participates in core-only/sanitizer validation as well as the normal Windows CTest matrix.

## Gates still open

- Exact-head Linux/Windows CI for `feat/m2-tempo-segment-editor-model`; do not merge this package until the final head is green.
- Native compact DeckPanel controls that bind this model to segment selection plus add/move/remove/BPM actions and persist the accepted owner grid through the existing source-identity-bound `TrackBeatGridOverrideStore` path.
- Real Windows 11 clean-machine/audio-interface validation, including device switching and independent four-output cue where supported.
- Manual resize/HiDPI usability review of the dense four-deck layout.
- Representative legal local-music corpus validation for BPM/key/grid behavior, especially genuinely variable-tempo material.
- Real listening/soak, controller and production-release qualification.
- Production key lock, slip/reverse/scratch and broader M2 workflow completion.

## Next largest step

Require exact-head CI for the editor-model branch first. If green, keep the package on its PR until the normal integration window and wire the tested model into a compact native segment editor: selected segment, add-at-current-position, move boundary, BPM edit and remove, followed by asynchronous persistence of the complete accepted reviewed grid. After that, validate real variable-tempo material before strengthening Sync or detector claims.
