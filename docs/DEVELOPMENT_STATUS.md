# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Verified default branch: `main` at `ad1191173d10a6954ec1c16cde1134d214f2fc16` (`M2: expose native Reverse and Slip deck controls (#41)`). Its post-merge GitHub Actions run `35489195912` completed successfully.
- Active development branch: `feat/m2-jog-scrub-stream-stress`.
- Active pull request: #42 (`M2: add bounded jog/scratch transport and reverse/slip stream stress`).
- Functional branch checkpoint before this status update: `288f5bf9aab5c44ac80b74d3470a0798acb5b256`.
- Exact-head CI run for that functional checkpoint: `35491326770`; it validates the code/test tree before this status-only checkpoint. The status update itself requires the normal final-head PR validation before merge.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA).
- No public BrokeDJ Release exists or is qualified by this checkpoint.

## Newly integrated on main: native Reverse / Slip controls

- Every deck exposes compact `REV` and `SLIP` controls through `PerformanceDeckOwner` and authoritative Engine atomics.
- During a real Slip Reverse split, the native deck shows audible versus hidden transport while the waveform follows the audible cursor.
- Reverse/Slip ownership remains fail-closed against reviewed Beat Loop and incompatible external/research renderer ownership.
- Main commit `ad1191173d10a6954ec1c16cde1134d214f2fc16` passed its post-merge Linux/Windows workflow run `35489195912`.

## Current PR #42: bounded jog/scratch transport foundation

1. **Audible signed platter velocity**
   - `JogScratchController` is JUCE-independent message/controller-thread code and reuses the production playback-rate plus Reverse transport instead of adding a second audio renderer.
   - A platter gesture owns ordinary transport only. Signed speed maps forward/reverse direction into the already qualified Engine rate envelope of 0.5x..1.5x; the deadzone holds the platter stopped.
   - Releasing the platter clears transient Reverse and restores the pre-gesture playing/rate snapshot.

2. **Precise bounded relocation**
   - Relative platter moves start from the authoritative audible cursor and publish through the existing last-request-wins seek mailbox.
   - Targets are clamped inside the loaded track. Existing Engine seek transitions perform the audio-side discontinuity handling; no new callback I/O, allocation, decoding or blocking synchronization is introduced.

3. **Ownership safety**
   - Existing Slip/Slip-Reverse and reviewed Beat Loop ownership reject scratch acquisition instead of being silently destroyed.
   - The initial slice intentionally does not claim wide-speed vinyl emulation, inertia, slip-scratch, jog-wheel UI/MIDI mapping or hardware-qualified feel.

## Current PR #42: long-track Reverse / Slip stress

- A deterministic core-only fixture represents a 90-minute stream using only the fixed `StreamCache`, without allocating full-track audio.
- The stress covers hidden-forward versus audible-reverse cursor divergence, release/rejoin, repeated direction changes, request bounds and finite output.
- Intentional reverse starvation must create exactly one new starvation episode; refill must close exactly one episode while output remains finite.
- These lock-free cache events are not presented as physical audio-interface underruns.
- This fixture does not replace compressed-codec slow-storage tests or real hardware listening; those remain separate evidence.

## Validation evidence and gates still open

- PR #42 must pass exact-final-head Linux ASan/UBSan + generated-progress + CTest and Windows x64 configure/build/full CTest/audio diagnostics/native GUI smoke/silent device probe/staging before merge.
- Real Windows 11 clean-machine/manual resize/HiDPI/device-switching validation remains open.
- Independent master 1/2 and cue 3/4 still require a real four-output interface and listening verification.
- Slow physical-storage and longer real compressed-codec Reverse/Slip stress remain open; the virtual stream fixture is deterministic core evidence, not hardware-underrun proof.
- Keyboard/MIDI mappings, native jog-wheel interaction and concrete controller profiles remain unqualified.
- Reviewed music-domain BPM/key/grid evidence, production key lock, wider scratch behavior, multi-hour soak and release qualification remain open.

## Next largest step

Fix any exact-head PR #42 regression first and merge only after the complete final head is green and the package is coherent. After that, connect the bounded jog/scratch owner to native deck/controller input with PL/EN state feedback, then extend slow-reader compressed-codec stress without weakening the realtime callback contract. M1 physical hardware gates continue whenever real device evidence is available; they are never replaced with CI simulation.
