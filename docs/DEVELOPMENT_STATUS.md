# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Verified default-branch baseline: `main` at `f153c3e8993516911b45a6db49ce6702b0ce899c` (`M2: add reverse and slip transport core (#39)`). PR #39 was merged only after its exact-head Linux/Windows gate passed.
- Post-merge `main` workflow: GitHub Actions run `35483076356` was started for `f153c3e8993516911b45a6db49ce6702b0ce899c`; its result is tracked separately from the already-green PR #39 gate.
- Active development branch: `feat/m2-reverse-slip-native-ui`, PR #40 (`M2: harden reverse/slip ownership and streamed playback`).
- Last fully green exact-head PR checkpoint before this commit: `3db13bc8410fdbd0930656855625d188e319d3cf`, GitHub Actions run `35483761664` (`success`). The current checkpoint extends that same PR and must pass a new exact-head run before merge.
- Active package: Reverse/Slip message-thread ownership, named four-state controller transitions and direction-aware bounded read-ahead for streamed reverse playback.
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

## Current PR #40

### Performance ownership hardening

- `PerformanceDeckOwner` publishes Reverse and Slip intent through the existing lock-free Engine controls and exposes the actual atomics back to UI/controller callers.
- The controller boundary now has one named four-state `ReverseSlipMode` (`forward`, `reverse`, `slipArmed`, `slipReverse`) and one `setReverseSlipMode()` transition entry point, so native UI/MIDI code can share the same rules instead of manually toggling two independent flags.
- Entering Slip Reverse publishes Slip before Reverse; leaving a split cursor drops Reverse before disarming Slip. The realtime Engine remains authoritative and can still clear incompatible requests fail-closed.
- Enabling a non-forward mode while a reviewed beat-loop region owns transport is rejected transactionally; no conflicting final control state is published.
- Whole-track LOOP remains compatible because it uses the complete immutable clip boundary rather than a separate reviewed beat region.
- Hot Cue trigger, Beat Jump and one-shot Sync clear Reverse/Slip before publishing their explicit seek/rate plan so hidden/audible cursors cannot reinterpret those transport jumps.
- Clip replacement clears Reverse/Slip immediately at the performance-owner boundary.
- Existing `PerformanceDeckOwnerTests` exercise the refactored public Reverse/Slip setters, beat-loop rejection, whole-track compatibility, Hot Cue / Beat Jump / Sync exits, invalid-deck rejection and clip-reset cleanup; exact-head CI is still required for this checkpoint.

### Direction-aware streamed read-ahead

- The background `StreamingTrack` worker still prioritizes the exact requested chunk and both immediate neighbours first.
- Deeper prefetch follows the observed direction of changed requested chunks. Ordinary forward playback keeps forward deep read-ahead; a sustained reverse/slip cursor can build cache behind the playhead instead of repeatedly biasing work ahead of it.
- Direction inference lives entirely on the existing background decoder thread. No additional audio-callback work, lock, allocation or I/O was introduced.
- A one-off backward seek may temporarily bias deep prefetch backward, but the exact chunk and both interpolation neighbours remain first priority and the next changed request re-establishes travel direction.
- `DecoderTests` establishes a distant forward request, verifies forward deep prefetch, then moves the request backward and requires a deeper preceding chunk to become resident. The previous forward-only worker would not satisfy that regression fixture.

## Gates still open

- Exact-head Linux ASan/UBSan and Windows x64 CI for this newer PR #40 checkpoint; do not merge until all required checks for the final head are green.
- Native four-deck REV/SLIP buttons with PL/EN labels/tooltips and visible audible-vs-hidden slip transport state.
- Slow physical-storage and long real-codec reverse/slip stress; deterministic cache residency is not hardware-underrun proof.
- Keyboard/MIDI mappings, manual resize/HiDPI interaction review and controller qualification.
- Real Windows 11 clean-machine/audio-interface validation, including device switching and independent four-output cue where supported.
- Reviewed music-domain listening, representative legal BPM/key/grid corpus evidence, multi-hour soak and production-release qualification.
- Production key lock, scratch and the remaining M2 workflow.

## Next largest step

Fix any exact-head regression in PR #40 first. Once this checkpoint is green, wire compact native REV/SLIP controls on all four deck panels against `ReverseSlipMode`, keep their toggle state synchronized to the Engine fail-closed atomics, show audible versus hidden transport only while Slip actually splits the clocks, and notify the optional key-lock lifecycle so incompatible research rendering remains safely disarmed. Then extend reverse/slip validation to longer compressed-file fixtures and controlled slow-reader direction changes before claiming stronger long-track reliability.
