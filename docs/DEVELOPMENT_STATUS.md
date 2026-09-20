# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch: `main` at `46ad8992df5d3c8af0f288ee9adc17f15c9c3892` (`Merge PR #42: bounded jog/scratch transport and reverse/slip stress`). PR #42 was merged only after exact-head Linux/Windows run `35491372445` completed successfully for `48b73ca20eea9d55f8838a6b7360c26a58f3e0d8`.
- Active development branch: `feat/m2-jog-ownership-hardening`.
- Active pull request: #43 (`M2: harden jog/scratch ownership lifecycle`).
- Functional code/test checkpoint before this status-only update: `fced45ec7b67d62a18c5a019ad250c3d6fe4a5a6`.
- Final-head CI for PR #43 is pending; do not merge until the exact final branch head passes the required Linux sanitizer/progress/CTest and Windows x64 build/test/GUI-smoke/device-probe/staging gates.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA).
- No public BrokeDJ Release exists or is qualified by this checkpoint.

## Newly integrated on main: bounded jog/scratch and long-stream stress

1. **Production-transport jog/scratch foundation**
   - `JogScratchController` is JUCE-independent message/controller-thread code and reuses the production playback-rate plus Reverse transport instead of adding a second audio renderer.
   - Signed platter speed is bounded to the Engine's qualified 0.5x..1.5x range; deadzone input holds the platter stationary and relative relocation uses the authoritative audible cursor plus the existing seek mailbox.
   - Existing Slip/Slip-Reverse and reviewed Beat Loop ownership reject gesture acquisition fail-closed.

2. **Deterministic Reverse/Slip long-stream stress**
   - A fixed-cache fixture represents 90 minutes without allocating the full track and exercises hidden-forward versus audible-reverse divergence, release/rejoin, repeated direction changes, request bounds and finite output.
   - Intentional starvation/refill is counted as one episode each. These cache diagnostics are not presented as physical audio-interface underruns.

3. **Validated integration boundary**
   - PR #42 exact-head run `35491372445` passed Linux ASan/UBSan + generated-progress + CTest and Windows x64 configure/build/full CTest/audio diagnostics/native no-audio GUI smoke/silent device probe/staging/artifact upload before merge.

## Current PR #43: jog/scratch ownership lifecycle hardening

1. **Revalidate ownership throughout the gesture**
   - Velocity and relative-move updates now re-check Slip and reviewed Beat Loop ownership instead of trusting only the state observed at `begin()`.
   - A Slip/loop action that arrives after platter touch cannot silently turn ordinary scratch into another transport mode or move the seek mailbox underneath the new owner.

2. **Do not resume stale playback after ownership takeover**
   - Normal `end()` restores the pre-gesture play/rate snapshot only when the scratch still owns ordinary transport.
   - If another owner appeared, release clears transient Reverse, preserves the foreign Slip/loop state, restores the user's rate, leaves playback stopped and reports `busy`.

3. **Fail-safe lifecycle abort**
   - Explicit `cancel()` supports clip replacement, controller disconnect, device/dialog teardown and shutdown without resuming playback.
   - Destructor cleanup fail-closes an abandoned active gesture so transient Reverse/playing state cannot leak into a later controller lifecycle.
   - Deterministic tests cover mid-gesture Slip takeover, mid-gesture Beat Loop takeover, forced cancel, repeated cancel and destructor cleanup.

## Validation evidence and gates still open

- PR #43 requires exact-final-head Linux ASan/UBSan + generated-progress + full CTest and Windows x64 configure/build/full CTest/audio diagnostics/native GUI smoke/silent device probe/staging before merge.
- Real Windows 11 clean-machine/manual resize/HiDPI/device-switching validation remains open.
- Independent master 1/2 and cue 3/4 still require a real four-output interface and listening verification.
- Slow physical-storage and longer real compressed-codec Reverse/Slip stress remain open; deterministic cache fixtures are not hardware-underrun proof.
- Native jog-wheel interaction, keyboard/MIDI mappings and concrete controller profiles remain unqualified.
- Reviewed music-domain BPM/key/grid evidence, production key lock, wider scratch behavior, multi-hour soak and public-release qualification remain open.

## Next largest step

Fix any exact-head PR #43 regression first and merge only after the complete final head is green and coherent. Then connect the hardened bounded jog/scratch owner to native deck/controller input with PL/EN state feedback and extend slow-reader compressed-codec Reverse/Slip stress. M1 physical hardware gates continue only with real device evidence; CI does not substitute for them.
