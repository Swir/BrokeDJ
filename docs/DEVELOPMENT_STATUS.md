# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Verified default-branch baseline: `main` at `3bbcfc6785e7b08066cd092018d1190737658500` (`M2: harden native variable-tempo map editing workflow (#38)`). Its post-merge validation was green.
- Active development branch: `feat/m2-reverse-slip-transport`, PR #39.
- Implementation checkpoint: `315c105a5aa67e579b3e2e8807c0012eb56a332c`; exact-head GitHub Actions run `35481031512` was started for that implementation head. Any later documentation/checkpoint commit on the PR requires its own exact-head run before merge.
- Active package: production-core reverse transport plus slip-reverse cursor separation, fail-closed compatibility boundaries and deterministic/realtime regression coverage. Native deck buttons/controller mappings are deliberately not claimed yet.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA).
- No public BrokeDJ Release exists or is qualified by this checkpoint.

## Newly integrated on main: native variable-tempo map editing

1. **Reviewed multi-segment tempo-map workflow**
   - The native deck can open the variable-tempo editor, select the segment at the playhead, add/move/remove later boundaries and edit segment BPM through the tested owner model.
   - Edits remain transactional, track/grid-bound and preserve segment-zero ownership rules.

2. **Recoverable editing and persistence**
   - Undo/Redo retains at most 16 complete reviewed-grid snapshots and invalidates stale history after track/grid replacement.
   - Accepted complete maps are persisted asynchronously through `TrackBeatGridOverrideStore`; no persistence work enters the audio callback.

3. **Verified integration boundary**
   - PR #38 merged to `main` as `3bbcfc6785e7b08066cd092018d1190737658500` after exact-head CI.
   - Hardware M1, representative music-corpus validation and live-performance gates remain open.

## Current development package: reverse and slip transport core

1. **Reverse transport semantics**
   - `Controls` now has lock-free `reverse` and `slip` flags sampled with the existing block control snapshot.
   - Ordinary reverse reads the production resampler backward while keeping transport and audible cursors together.
   - At track start, reverse stops when whole-track loop is off and wraps to the track end when whole-track loop is on.
   - Clip adoption clears transient reverse/slip state rather than carrying performance modes into a replacement track.

2. **Slip-reverse cursor ownership**
   - While reverse+slip is active, the underlying transport continues forward while the audible cursor moves backward.
   - Releasing reverse rejoins the uninterrupted hidden timeline through the existing prepared short transition instead of hard-jumping the output.
   - Meters expose the existing separate `position` and `audiblePosition` clocks so future UI/controller work can show the distinction explicitly.

3. **Fail-closed composition rules**
   - Beat-derived loop regions reject arming while reverse/slip is active. If reverse/slip is requested after a beat region is already armed, the incompatible transport modes are cleared before rendering that block.
   - External source renderers, including the opt-in key-lock research path, also clear reverse/slip until a combined transport contract is implemented and tested.
   - Whole-track loop remains supported because its bounds are the immutable complete clip rather than a separately owned beat-loop region.

4. **Realtime and quality regression coverage**
   - Core tests cover reverse direction, start-bound stop, whole-track reverse wrap, slip hidden/audible cursor divergence and rejoin, beat-loop incompatibility and replacement reset.
   - Quality coverage uses a polarity-step fixture to require the slip-release rejoin to begin from the previous audible side and settle onto the hidden transport through the prepared transition.
   - The existing zero-heap callback stress now cycles reverse-only, slip-only and reverse+slip states on a streamed deck while the second deck keeps beat-loop/control automation stress active.

## Gates still open

- Exact-head Linux ASan/UBSan and Windows x64 CI for the final PR #39 head; do not merge until all required checks for that exact head are green.
- Native four-deck reverse/slip controls, PL/EN labels/tooltips, keyboard/MIDI mapping and manual resize/HiDPI interaction review.
- Streamed long-track reverse/slip stress on slow physical storage and reviewed music-domain listening; cache diagnostics are not hardware underrun proof.
- Real Windows 11 clean-machine/audio-interface validation, including device switching and independent four-output cue where supported.
- Representative legal local-music corpus validation for BPM/key/grid behavior, especially genuinely variable-tempo material.
- Controller qualification, multi-hour soak and production-release qualification.
- Production key lock, scratch and broader M2 workflow completion.

## Next largest step

First fix any CI regression in PR #39 and keep the final head gated. Once the core reverse/slip semantics are exact-head green, expose them through compact native deck controls with PL/EN text, make the audible-vs-hidden slip state visible without clutter, and add GUI smoke/lifecycle coverage. Do not mark M2 complete until the remaining deck-performance and quality gates are satisfied.
