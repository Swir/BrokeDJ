# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch: `main` at `ad7345445f779163fc8ae542969a0086d09f01c1` (`M2: native jog + delayed reverse/slip streaming hardening`). PR #44 was merged only after exact-head run `35500293369` completed successfully for `47dcccd182d5a349cf36309c9220dc16e9b354c2`.
- Active development branch: `feat/m2-keylock-transition-hardening`.
- Active pull request: #45 (`M2: harden key-lock transport and device transitions`).
- Previous exact-head checkpoint `dbdc7619bfc726601d4be57d0986895aeef2c670` passed Build and test run `35502264874` across the configured Linux and Windows gates.
- Latest implementation/test checkpoint before this status update: `d7dbf48669ecf03249dda534b20335a282545f70` (`M2: harden key-lock control validity and owner disarm`). This newer checkpoint requires a fresh exact-final-head gate before merge.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA).
- GitHub Releases is empty; no public BrokeDJ Release exists or is qualified by this checkpoint.

## Newly integrated on main: native jog and delayed streaming hardening

1. **Native bounded JOG/SCRATCH on every deck**
   - The compact spring-to-centre control reuses the hardened production transport owner, keeps PL/EN state feedback and fails closed around Slip/Beat Loop ownership.
   - Resize/lifecycle recovery avoids cumulative waveform shrink and accidental mouse-wheel edits.

2. **Last-request-wins delayed read-ahead**
   - The background reader rechecks the requested chunk during controlled delay and before another decoder read, allowing a seek/reverse direction change to abandon stale delayed work without interrupting an in-progress third-party decoder call.

3. **Real decoder stress gate**
   - Generated FLAC/OGG forced-streaming Reverse/Slip tests cover split cursors, finite output, starvation/refill closure and backward prefetch.
   - PR #44 exact-head run `35500293369` passed the required Linux sanitizer/progress/CTest and Windows x64 build/test/audio-diagnostics/native GUI-smoke/device-probe/staging gates before merge.

## Current PR #45: key-lock transition and control-validity hardening

1. **Authoritative transport snapshot validation**
   - `KeyLockDeckLifecycle::service()` compares current loop/rate controls with the last successfully staged key-lock snapshot.
   - A controller or device path that changes those Engine controls without calling the UI notification hook is detected off-callback, disarmed fail-closed and marked dirty for a later paused restage instead of remaining permanently stale.
   - Snapshot comparison tolerates the native float-control to double-service round trip, so a stable 1.10x rate does not create repeated stage/generation churn.

2. **Sticky invalid-pitch fail-closed barrier**
   - An out-of-range or non-finite pitch request now latches the optional key-lock path invalid instead of leaving the timer free to silently re-arm the last valid pitch while paused.
   - A later valid pitch request explicitly clears the barrier. Re-applying the same previous valid value is sufficient, so recovery does not require fabricating a different pitch value solely to trigger restaging.
   - Deterministic lifecycle coverage checks both playing and paused invalid-control service passes, proves the owner remains disarmed, then verifies explicit valid recovery and restaging.

3. **Idempotent owner disarm and production-owner restoration**
   - `EngineKeyLockDeckOwner::disarm()` now advances its publication generation only when an active optional source actually transitions to fallback. Repeated disabled/timer polling no longer manufactures generation churn.
   - Lifecycle tests disable the owner, call the disabled service path repeatedly and require the generation to remain stable after the first real transition.
   - Removing an optional/research deck source with `setDeckSourceRenderer(deck, nullptr)` still restores Engine's built-in loop-region renderer, preserving production Beat Loop and Reverse/Slip ownership after research teardown.

4. **Realtime boundary remains conservative**
   - All new validation is on the serialized non-audio owner path. No decoder work, file/network I/O, blocking mutex, allocation or unbounded work was added to `Engine::process()`.
   - Production playback remains the immediate fallback whenever the optional path is invalid, dirty, unavailable or deliberately disarmed.

## Validation evidence and gates still open

- PR #45 now requires a fresh exact-final-head Linux ASan/UBSan + generated-progress + full configured CTest and Windows x64 configure/build/full CTest/audio diagnostics/native GUI smoke/silent device probe/staging after checkpoint `d7dbf48669ecf03249dda534b20335a282545f70` and this status update.
- Real Windows 11 clean-machine/manual resize/HiDPI/device-switching validation remains open.
- Independent master 1/2 and cue 3/4 still require a real four-output interface and listening verification.
- Physical slow-storage, multi-minute real-world compressed files and hardware underrun behavior remain open; controlled delayed decoder fixtures are not physical-storage qualification.
- Native MIDI/controller mappings and concrete controller profiles remain unqualified; the current native jog input is mouse/native-GUI only.
- Reviewed music-domain BPM/key/grid evidence, production key-lock listening/latency, wider scratch behavior, multi-hour soak and public-release qualification remain open.

## Next largest step

Run the complete exact-head CI gate for the final PR #45 head and fix any regression before integration. If green, keep key lock explicitly experimental until reviewed listening/latency evidence exists; continue with deterministic multi-deck control/device transition coverage and the remaining M2 performance-deck gaps rather than treating source CI as hardware qualification.
