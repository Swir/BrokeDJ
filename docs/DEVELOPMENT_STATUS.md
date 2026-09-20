# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch: `main` at `55299b6c97ea72edc5ee97a2baf124e7cbeef3ff` (`Merge PR #43: harden jog/scratch ownership lifecycle`). PR #43 was merged only after exact-head run `35493724750` completed successfully for `fb5eb49f338c1b33bab38428e301bdbc9b125896`.
- Active development branch: `feat/m2-native-jog-strip`.
- Active pull request: #44 (`M2: expose bounded native jog/scratch strip`).
- Functional native-control checkpoint before this status update: `b7c6e31c5952e79cfa5c51f425d7062cd209148f`; its run `35496535425` was queued when this checkpoint was written. The final status commit requires a fresh exact-head gate before merge.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA).
- No public BrokeDJ Release exists or is qualified by this checkpoint.

## Newly integrated on main: hardened bounded jog/scratch owner

1. **Production-transport platter ownership**
   - `JogScratchController` reuses the production playback-rate plus Reverse transport rather than adding a second audio renderer.
   - Signed speed is bounded to the Engine's qualified 0.5x..1.5x range; deadzone input holds the platter stationary and relative moves use the audible cursor plus the existing seek mailbox.

2. **Ownership changes fail closed**
   - Slip and reviewed Beat Loop ownership are revalidated throughout an active gesture, not only at acquisition.
   - A foreign owner arriving mid-gesture cannot make scratch relocate the seek mailbox or silently resume stale playback on release.

3. **Lifecycle abort**
   - Explicit `cancel()` and destructor cleanup stop abandoned gestures without leaking transient Reverse/playing state into controller/device teardown.
   - PR #43 exact-head Linux sanitizer/progress/CTest and Windows x64 build/test/GUI-smoke/device-probe/staging run `35493724750` passed before merge.

## Current PR #44: native jog/scratch deck strip

1. **Usable control on every native deck**
   - Each `DeckPanel` mounts a compact spring-to-centre JOG/SCRATCH strip backed by the hardened `JogScratchController`.
   - The strip takes height from the waveform instead of overlaying existing performance/grid controls, preserving the current responsive deck layout.

2. **PL/EN state and fail-closed feedback**
   - The control reports ready/active/blocked/no-track states and provides a bilingual scope tooltip that does not claim hardware-qualified vinyl feel.
   - Small non-zero slider movement snaps to the controller's actual 0.5x minimum instead of displaying a speed the Engine cannot render.

3. **Message-thread lifecycle monitoring**
   - A held gesture aborts if the published track duration changes, Slip/Beat Loop takes ownership, or realtime processing rejects requested Reverse.
   - No decoding, file/network I/O, allocation, blocking lock or new unbounded work is added to `Engine::process()`.

## Validation evidence and gates still open

- PR #44 requires exact-final-head Linux ASan/UBSan + generated-progress + full CTest and Windows x64 configure/build/full CTest/audio diagnostics/native GUI smoke/silent device probe/staging before merge.
- Real Windows 11 clean-machine/manual resize/HiDPI/device-switching validation remains open.
- Independent master 1/2 and cue 3/4 still require a real four-output interface and listening verification.
- Slow physical-storage and longer real compressed-codec Reverse/Slip stress remain open; deterministic cache fixtures are not hardware-underrun proof.
- Native MIDI/controller mappings and concrete controller profiles remain unqualified; this PR is mouse/native-GUI input only.
- Reviewed music-domain BPM/key/grid evidence, production key lock, wider scratch behavior, multi-hour soak and public-release qualification remain open.

## Next largest step

Fix any exact-head PR #44 regression first and merge only after the complete final head is green and coherent. After native jog interaction is safely integrated, extend slow-reader compressed-codec Reverse/Slip stress and then move M2 toward a production key-lock decision based on reviewed listening/race/device-transition evidence. M1 physical hardware gates remain truthful and require real devices; CI does not substitute for them.
