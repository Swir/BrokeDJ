# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Verified default-branch baseline: `main` at `f153c3e8993516911b45a6db49ce6702b0ce899c` (`M2: add reverse and slip transport core (#39)`). PR #39 was merged only after its exact-head Linux/Windows gate passed.
- Post-merge `main` workflow: GitHub Actions run `35483076356` was started for `f153c3e8993516911b45a6db49ce6702b0ce899c`; its result must be treated separately from the already-green PR gate.
- Active development branch: `feat/m2-reverse-slip-native-ui`, PR #40.
- Code checkpoint before this documentation update: `01b738236e56fec698cabef5de87b82d3197af7f`; PR run `35483624952` was queued for that head. This documentation commit creates a newer PR head and therefore requires its own exact-head checks before any merge.
- Active package: message/control-thread ownership rules for Reverse/Slip, transactional incompatibility with reviewed beat-loop regions, and safe exit from split-cursor mode before explicit Hot Cue / Beat Jump / Sync transport moves.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA).
- No public BrokeDJ Release exists or is qualified by this checkpoint.

## Newly integrated on main: reverse and slip transport core

1. **Reverse transport**
   - The production Engine can read the existing bounded resampler backward without changing the dependency stack.
   - Reverse stops at track start when whole-track loop is off and wraps across the complete immutable track when whole-track loop is on.
   - Transport changes use the prepared transition system rather than adding callback allocation or I/O.

2. **Slip-reverse clock separation**
   - Reverse+Slip keeps an uninterrupted hidden transport moving forward while the audible cursor moves backward.
   - Releasing Reverse rejoins the hidden timeline through the prepared transition.
   - `Meter::position` and `Meter::audiblePosition` remain separate so the native UI can expose the distinction without inventing state outside the Engine.

3. **Fail-closed realtime boundaries**
   - Reviewed beat-loop regions and external/research renderers do not silently combine with Reverse/Slip.
   - Deterministic quality and realtime-contract coverage exercises reverse direction, slip rejoin and streamed callback stress while preserving the zero-heap measured-window requirement.

## Current PR #40: performance ownership hardening

1. **Owned Reverse/Slip intent**
   - `PerformanceDeckOwner` now publishes Reverse and Slip intent through the existing lock-free Engine controls and exposes the actual atomics back to UI/controller callers.
   - Enabling either mode while a reviewed beat-loop region owns transport is rejected transactionally; no conflicting control state is published.
   - Whole-track LOOP remains compatible because it uses the complete immutable clip boundary rather than a separate reviewed beat region.

2. **Explicit transport jumps exit split-cursor mode**
   - Hot Cue trigger, Beat Jump and one-shot Sync clear Reverse/Slip before publishing their explicit seek/rate plan.
   - Clip replacement clears Reverse/Slip immediately at the performance-owner boundary instead of waiting for the callback to observe a new clip.

3. **Deterministic regression coverage**
   - `PerformanceDeckOwnerTests` now covers Reverse/Slip activation, beat-loop rejection, whole-track compatibility, Hot Cue / Beat Jump / Sync exits, invalid-deck rejection and clip-reset cleanup.
   - This is owner/control-boundary evidence only. External key-lock/render-path composition is still deliberately fail-closed in the Engine and not claimed as combined support.

## Gates still open

- Exact-head Linux ASan/UBSan and Windows x64 CI for the final PR #40 head; do not merge until all required checks for that exact head are green.
- Native four-deck REV/SLIP buttons with PL/EN labels/tooltips and visible audible-vs-hidden slip transport state.
- Reverse-aware long-track read-ahead / slow-storage stress. The current worker prioritizes the requested chunk and immediate neighbours but deeper prefetch is still forward-biased.
- Keyboard/MIDI mappings, manual resize/HiDPI interaction review and controller qualification.
- Real Windows 11 clean-machine/audio-interface validation, including device switching and independent four-output cue where supported.
- Reviewed music-domain listening, representative legal BPM/key/grid corpus evidence, multi-hour soak and production-release qualification.
- Production key lock, scratch and the remaining M2 workflow.

## Next largest step

First fix any exact-head CI regression in PR #40. Once that owner boundary is green, wire compact native REV/SLIP controls on all four deck panels, keep their toggle state synchronized to the Engine fail-closed atomics, show audible versus hidden transport only when Slip actually splits the clocks, and notify the optional key-lock lifecycle so incompatible research rendering remains safely disarmed. In parallel, harden long-track read-ahead for sustained reverse playback rather than assuming forward-only deep prefetch.
