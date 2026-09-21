# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `9828b638687ac5da4a3cab019d433cc4ec0c75f0`, the merge commit for PR #73 (`add privacy-safe M2 listening witness`).
- PR #73 exact-final-head `7f7fc016df00317718c7dd854659ce2e7d31cc4c` passed Build and test `35625131253`, M2 key-lock listening witness tool `35625131239`, M1 hardware witness tool `35625131302` and Native library scale smoke `35625131295` before merge.
- Merged-main verification for `9828b638…` is still running at this checkpoint: Build and test `35630102960` and Native library scale smoke `35630103149`. Do not assume merged-main green until GitHub reports completion.
- Active development: draft PR #74, branch `feat/m3-mixer-recording-witness`. Functional/package head before this checkpoint documentation commit: `89427761d00007dede232f0d327fbcc3c48228cc`.
- PR #74 adds a privacy-minimized Windows 11 x64 M3 mixer/recording qualification recorder bound to the exact `BrokeDJ.exe`. It requires at least 60 continuous minutes, at least three user-owned/licensed representative tracks and explicit checks for EQ listening, gain staging, crossfader laws, limiter behavior, microphone/ducking, Booth 5/6 routing, cue isolation, recording playback, zero recording dropouts and runtime-error review.
- The M3 evidence schema excludes device names, track names/paths, recording paths, source music and microphone audio. Generation refuses CI, while validation rejects incomplete, type-spoofed, privacy-unsafe, unexpected-field, wrong-executable and wrong-environment evidence.
- Windows development packaging now stages `M3-MIXER-RECORDING-WITNESS.ps1` and `M3-MIXER-RECORDING-WITNESS.md` beside the exact staged executable and includes them in the existing package manifest/checksum contract. Downloaded-package smoke verifies both files and proves the packaged recorder cannot mint M3 evidence under CI.
- PR #74 functional head queued Build and test `35630751002`, Native library scale smoke `35630751073`, M3 mixer recording witness tool `35630751187`, M2 key-lock listening witness tool `35630751027` and M1 hardware witness tool `35630751118`; all were still running when this checkpoint was written.
- This checkpoint commit changes the PR head, so fresh exact-final-head checks remain mandatory before integration even if the functional-head runs finish green.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). Tooling does not advance M3 without real Windows 11 listening/hardware/recording evidence.
- GitHub Releases remains empty; no public BrokeDJ alpha, beta or stable release is qualified.

## Integrated foundations on main

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, a silent device-capability probe and a packaged privacy-safe hardware witness recorder. Real clean-machine playback, device switching and physical master 1/2 versus CUE 3/4 isolation remain manual.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH, the opt-in Signalsmith-backed key-lock research lifecycle and now a packaged privacy-safe listening witness recorder. Real representative listening/latency/hardware evidence remains open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, opt-in microphone ducking, limiter measurements and deterministic EQ qualification; physical/listening gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe backup/restore and native 5k-track/12k-history staged-EXE recovery.

## Gates still open

- M1: real Windows 11 clean launch/resize/import/playback/device-switch/four-output-cue witness on actual hardware.
- M2: representative music-domain BPM/key evidence, real key-lock listening evidence, device CPU/callback-deadline/underrun and latency qualification, plus controller/wider scratch qualification required by the milestone.
- M3: real Windows 11 reviewed EQ/mixer listening plus physical microphone/ducking/Booth/recording/dropout qualification and later long-session evidence beyond the focused witness.
- M4: real Windows 11 connected-library/session witness from the exact staged executable.
- Public release qualification remains a later M9 gate; green development CI alone does not authorize a release.

## Next largest step

Let the fresh exact-final-head PR #74 checks complete and fix every regression before integration. If the package becomes green, the internally closable M3 blocker-removal tooling is complete; the remaining M1–M4 gates above require real user-controlled Windows 11 listening/hardware/workflow evidence rather than more same-version feature expansion.
