# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying the already-implemented M1–M4 core workflow before unrelated feature expansion. Optional M5–M8 breadth remains later work unless a concrete Beta blocker requires it.

## Current checkpoint

- Default branch baseline: `main` at `aadfca0630494e6d60c4c7dc9cc91c87e96d73e8`, the squash merge of PR #75 (`Add exact-executable first Beta qualification gate`).
- GitHub Releases remains empty. Active development continues in draft PR #76 on branch `feat/pro-ui-pass`; no competing BrokeDJ issue/PR was present when this checkpoint was prepared.
- PR head `01c1bf5ce8b466a9929da8eba313d6c8a9112feb` passed every exact-head workflow that currently exercises this UI package: Build and test `35728322952`, Native library scale smoke `35728322972`, UI visual witness `35728322976`, M1 hardware witness tool `35728322950`, M2 key-lock witness tool `35728322962`, M3 mixer/recording witness tool `35728322956` and Beta qualification tool `35728322958`.
- The successful UI witness captured both the compact 1050×800-class layout and the workstation-class layout from the native Windows executable. Review confirmed that the central mixer/deck columns are structurally separated and captured, but also exposed release-critical visual debt: the rotary controls read too small in the central mixer, disabled controls are too faint, and the control surfaces still read more like a development panel than a finished workstation.
- This checkpoint therefore keeps the same central-mixer geometry and audio ownership but replaces the global control skin with a denser professional pass: JUCE's already-calculated rotary bounds are no longer reduced a second time for the text box, knobs become materially larger, fader caps/slots are clearer, disabled labels retain usable contrast, and buttons/combos use a flatter restrained workstation treatment. No Engine callback, routing, decoder, device lifecycle, recording or DSP code is changed.
- Fresh exact-head CI and a new Windows UI witness are required after this visual checkpoint. The new PNGs must be inspected before the PR can be considered visually stable; hosted Windows Server screenshots still do not certify real Windows 11 HiDPI quality, manual usability or live performance.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. UI restructuring, polish and qualification tooling do not advance the milestone counter by themselves.

## Integrated foundations on main

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, a silent device-capability probe and a packaged privacy-safe hardware witness recorder. Real clean-machine playback, device switching and physical master 1/2 versus CUE 3/4 isolation remain manual.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH, the opt-in Signalsmith-backed key-lock research lifecycle and a packaged privacy-safe listening witness recorder. Representative music-domain and real listening/latency/hardware evidence remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, opt-in microphone ducking, limiter measurements, deterministic EQ qualification and a packaged mixer/recording witness. Physical/listening gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe backup/restore and native 5k-track/12k-history staged-EXE recovery.
- The merged Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace those human hardware/listening workflows and does not make a public Beta Release by itself.

## Gates still open before first Beta qualification

- M1: real Windows 11 clean launch/resize/import/playback/device-switch/four-output-cue witness on actual hardware.
- M2: representative music-domain BPM/key evidence, real key-lock listening evidence, device CPU/callback-deadline/underrun and latency qualification, plus controller/wider scratch qualification required by the current milestone wording.
- M3: real Windows 11 reviewed EQ/mixer listening plus physical microphone/ducking/Booth/recording/dropout qualification and long-session evidence required by the witness.
- M4: real Windows 11 connected-library/session witness from the exact staged executable.
- UI: PR #76 needs the new exact-final-head build/package/scale workflows and Windows UI pixel witness to pass, followed by real Windows 11 review at the normal workstation canvas and the 1050×800 compact minimum.
- Public Beta publication remains blocked until the applicable qualification evidence, exact candidate CI/package checks, source/notices/checksums and known-issues review are complete. No public release is authorized yet.

## Next largest step

Run the exact-head Windows build and UI witness for this control-skin checkpoint, inspect both new PNG artifacts and fix any legibility, clipping or density regression in the same PR. Once the UI branch is visually acceptable, return to the frozen Beta gates: the real Windows 11 M1–M4 hardware/listening/library-session qualification rather than widening feature scope.
