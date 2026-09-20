# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch: `main` at `ad7345445f779163fc8ae542969a0086d09f01c1` (`M2: native jog + delayed reverse/slip streaming hardening`). PR #44 was merged only after exact-head run `35500293369` completed successfully for `47dcccd182d5a349cf36309c9220dc16e9b354c2`.
- Active development branch: `feat/m2-keylock-transition-hardening`.
- Active pull request: pending creation for the key-lock transition package in this checkpoint.
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

## Current development: key-lock transition hardening

1. **Authoritative transport snapshot validation**
   - `KeyLockDeckLifecycle::service()` now compares the current loop/rate controls with the last successfully staged key-lock snapshot.
   - A controller or device path that changes those Engine controls without calling the UI notification hook is detected off-callback, disarmed fail-closed and marked dirty for a later paused restage instead of remaining permanently stale.

2. **Clip and device-transition recovery tests**
   - Deterministic lifecycle coverage now exercises unnotified live rate and loop changes, live clip replacement, failed device configuration and a later valid device re-prepare.
   - The optional renderer remains fallback-first while playing; staging still occurs only on the serialized non-audio owner path while the deck is paused.

3. **Realtime boundary unchanged**
   - This package adds no I/O, decoding, allocation, blocking mutex or unbounded work to `Engine::process()`; it only strengthens message/control-thread lifecycle validation and tests.

## Validation evidence and gates still open

- The active key-lock transition branch requires exact-head Linux ASan/UBSan + generated-progress + full configured CTest and Windows x64 configure/build/full CTest/audio diagnostics/native GUI smoke/silent device probe/staging before any merge.
- Real Windows 11 clean-machine/manual resize/HiDPI/device-switching validation remains open.
- Independent master 1/2 and cue 3/4 still require a real four-output interface and listening verification.
- Physical slow-storage, multi-minute real-world compressed files and hardware underrun behavior remain open; controlled delayed decoder fixtures are not physical-storage qualification.
- Native MIDI/controller mappings and concrete controller profiles remain unqualified; the current native jog input is mouse/native-GUI only.
- Reviewed music-domain BPM/key/grid evidence, production key-lock listening/latency, wider scratch behavior, multi-hour soak and public-release qualification remain open.

## Next largest step

Run the complete exact-head CI gate for the key-lock transition package and fix any regression before integration. If green, keep production key lock explicitly experimental until reviewed listening/latency evidence exists, then continue with deterministic device/control races and the next M2 performance-deck gaps rather than treating source CI as hardware qualification.
