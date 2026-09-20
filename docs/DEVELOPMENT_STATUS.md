# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch: `main` at `55299b6c97ea72edc5ee97a2baf124e7cbeef3ff` (`Merge PR #43: harden jog/scratch ownership lifecycle`). PR #43 was merged only after exact-head run `35493724750` completed successfully for `fb5eb49f338c1b33bab38428e301bdbc9b125896`.
- Active development branch: `feat/m2-native-jog-strip`.
- Active pull request: #44 (`M2: expose bounded native jog/scratch strip`).
- Native jog checkpoint `84e6c87c04bfdcf13e23edf15701c6d51c044c2f` passed exact-head run `35496555186` across Linux sanitizer/progress/CTest and Windows x64 build/test/audio-diagnostics/native GUI-smoke/device-probe/staging. This branch now also hardens delayed read-ahead preemption and adds generated FLAC/OGG Reverse/Slip streaming stress; the new final head requires its own exact-head gate before merge.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA).
- GitHub Releases is empty; no public BrokeDJ Release exists or is qualified by this checkpoint.

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

## Current PR #44: native jog plus slow-reader transport hardening

1. **Usable control on every native deck**
   - Each `DeckPanel` mounts a compact spring-to-centre JOG/SCRATCH strip backed by the hardened `JogScratchController`.
   - The strip takes height from the waveform instead of overlaying existing performance/grid controls, preserving the current responsive deck layout.
   - The control reports PL/EN ready/active/blocked/no-track states and does not claim hardware-qualified vinyl feel.

2. **Last-request-wins inside delayed read-ahead waits**
   - The background `StreamingTrack` now re-checks the authoritative requested chunk during the bounded artificial/slow-reader delay and immediately before starting a decoder read.
   - A seek or rapid Reverse direction change can therefore abandon stale delayed prefetch before another codec read starts. Decoder work stays off the audio callback; a codec read already in progress is allowed to finish safely rather than attempting unsafe third-party decoder interruption.

3. **Real decoder Reverse/Slip stress**
   - A new app-adapter CTest generates original six-second FLAC and OGG fixtures, forces streaming through the production JUCE decoder, injects controlled read-ahead delay, drives Engine Slip Reverse faster than the worker can refill and verifies finite output, split hidden/audible cursors, starvation/refill closure and backward directional prefetch.
   - A separate delayed-FLAC case exercises in-flight stale-prefetch abandonment. These fixtures strengthen automated compressed-codec evidence but do not emulate every physical disk, filesystem, codec file or OS scheduling sequence.

## Validation evidence and gates still open

- The new final head of PR #44 requires exact-head Linux ASan/UBSan + generated-progress + full CTest and Windows x64 configure/build/full CTest/audio diagnostics/native GUI smoke/silent device probe/staging before merge.
- Real Windows 11 clean-machine/manual resize/HiDPI/device-switching validation remains open.
- Independent master 1/2 and cue 3/4 still require a real four-output interface and listening verification.
- Physical slow-storage, multi-minute real-world compressed files and hardware underrun behavior remain open; controlled delayed decoder fixtures are not physical-storage qualification.
- Native MIDI/controller mappings and concrete controller profiles remain unqualified; the current native jog input is mouse/native-GUI only.
- Reviewed music-domain BPM/key/grid evidence, production key lock, wider scratch behavior, multi-hour soak and public-release qualification remain open.

## Next largest step

Fix any exact-head PR #44 regression first. Keep the branch open until the complete final head is green and the normal integration window is appropriate; do not merge merely because one hourly checkpoint passed. After this transport/read-ahead package is qualified, the next large M2 decision is production key-lock hardening based on reviewed listening plus race/device-transition evidence, while M1 physical hardware gates remain truthful and require real devices.
