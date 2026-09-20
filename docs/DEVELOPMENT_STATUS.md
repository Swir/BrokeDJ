# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Verified default branch: `main` at `b49d0867037fc04cc2c8db452a0189b54858f631` (`M2: harden reverse/slip ownership and streamed playback (#40)`). PR #40 was merged only after exact-head Linux/Windows run `35486306333` completed successfully.
- Active development branch: `feat/m2-native-reverse-slip-controls`.
- Active pull request: #41 (`M2: expose native Reverse and Slip deck controls`).
- Functional code checkpoint before this status-only update: `e0c3797380aa51a97684534439d2c7f388285bf0`.
- Exact-head CI for PR #41: pending; do not merge until the final branch head passes the required Linux sanitizer/progress gate and Windows x64 build/test/GUI-smoke/device-probe/staging gate.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA).
- No public BrokeDJ Release exists or is qualified by this checkpoint.

## Newly integrated on main: reverse/slip ownership and streamed playback hardening

1. **Authoritative four-state transport ownership**
   - `PerformanceDeckOwner::ReverseSlipMode` names `forward`, `reverse`, `slipArmed` and `slipReverse` and publishes those intents through one message/controller-thread boundary.
   - Entering Slip Reverse publishes Slip before Reverse; leaving a split cursor removes Reverse before disarming Slip.
   - Beat Loop ownership rejects non-forward Reverse/Slip transactionally, while whole-track LOOP remains compatible.

2. **Explicit performance-action exits**
   - Hot Cue trigger, Beat Jump and one-shot Sync clear Reverse/Slip before publishing their own transport plans.
   - Clip replacement also clears the transient mode, preventing state from leaking into the next immutable clip.

3. **Direction-aware bounded streaming read-ahead**
   - The background streaming worker prioritizes the requested chunk and immediate neighbours, then biases deeper prefetch in the observed travel direction.
   - Reverse playback can therefore build cache behind its current source cursor without adding disk I/O, decoding, blocking locks or unbounded work to the audio callback.

## Current PR #41: native four-deck REV / SLIP controls

- Every deck panel now has compact `REV` and `SLIP` controls wired to the existing `PerformanceDeckOwner` rather than directly inventing a second transport state machine in the GUI.
- Button state refreshes from the authoritative Engine atomics. If the realtime Engine rejects an incompatible renderer/mode fail-closed, the UI follows the actual state instead of remaining visually latched.
- Requests are rejected when no playable track exists; reviewed Beat Loop prevents REV/SLIP controls from claiming the same deck transport.
- The optional developer key-lock research lifecycle is notified before entering a non-forward Reverse/Slip mode so the research renderer can disarm and ordinary production playback remains the deterministic fallback.
- During an actual Slip Reverse clock split, the deck displays both the audible source cursor and hidden forward transport (`AUD / HIDDEN`, `SŁYSZ / UKRYTY`). The waveform playhead follows the audible cursor during that split and otherwise follows normal transport.
- English/Polish tooltips and status messages explain Reverse, Slip and the hidden transport without claiming scratch, continuous sync, hardware qualification or production key lock.
- No decoder, persistence, filesystem or network work was added to the audio callback.

## Validation evidence and gates still open

- PR #41 exact-final-head CI is pending. A successful earlier #40 gate does not validate this GUI integration.
- Native no-audio GUI smoke can validate construction/lifecycle of the new controls, but it does not prove audible Reverse/Slip behavior on a physical interface.
- Slow physical-storage and long real-codec reverse/slip stress remain open; deterministic cache residency is not hardware-underrun proof.
- Real Windows 11 clean-machine/manual resize/HiDPI/device-switching validation remains open.
- Independent master 1/2 and cue 3/4 still require a real four-output interface and listening verification.
- Keyboard/MIDI mappings and concrete controller profiles remain unqualified.
- Reviewed music-domain listening, representative legal BPM/key/grid corpus evidence, multi-hour soak and release qualification remain open.
- Production key lock, scratch and the remaining M2 workflow are not complete.

## Next largest step

Fix any exact-head PR #41 regression first. Once the native REV/SLIP integration is green, harden longer compressed-track reverse/slip behavior under controlled slow read-ahead and then advance the performance transport toward a bounded scratch/jog model without weakening the realtime callback contract. M1 physical hardware gates continue in parallel whenever real device evidence is available; they must not be replaced with CI simulation.
