# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `bef37252eb97a9bd3d3d465eec1665f083a1c1dc`, the merge commit for PR #72 (`ship self-contained M1 hardware witness`).
- Merged-main verification for that exact commit is green: Build and test `35619027328` and Native library scale smoke `35619027348` both completed successfully.
- GitHub Releases is still empty. There is no qualified public BrokeDJ alpha, beta or stable release.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). M1 and M4 remain open because their documented real Windows 11 manual witnesses have not been performed.
- Active development branch: `feat/m2-keylock-listening-witness`. This branch adds a privacy-minimized M2 human listening evidence recorder and its dedicated negative-fixture CI. The recorder binds evidence to the exact `BrokeDJ.exe`, requires at least three user-owned/licensed representative tracks, requires both slow and fast key-lock reviews plus fallback/error review, and refuses to generate evidence under CI.
- The M2 witness deliberately does not launch BrokeDJ, select music, change volume or claim that listening occurred. CI may validate schema fixtures only. Production key lock remains blocked on real representative-material listening plus device CPU/callback-deadline/underrun and latency evidence.
- The M2 witness tool is not yet staged into the Windows development artifact at this checkpoint; package integration is intentionally deferred until the new script/schema workflow is green on its exact branch head.
- Exact-head PR/run identifiers are recorded after the branch PR is opened; this checkpoint must be refreshed if the branch head changes.

## Integrated foundations on main

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, a silent device-capability probe and a packaged privacy-safe hardware witness recorder. Real clean-machine playback, device switching and physical master 1/2 versus CUE 3/4 isolation remain manual.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH, and the opt-in Signalsmith-backed key-lock research lifecycle with deterministic ratio/pitch/fallback and warmed zero-heap tests. It is not a production key-lock claim.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, opt-in microphone ducking, limiter measurements and deterministic EQ qualification; physical/listening gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe backup/restore and native 5k-track/12k-history staged-EXE recovery.

## Gates still open

- M1: real Windows 11 clean launch/resize/import/playback/device-switch/four-output-cue witness on actual hardware.
- M4: real Windows 11 connected-library/session witness from the exact staged executable.
- M2: representative music-domain BPM/key evidence, real key-lock listening evidence, device CPU/callback-deadline/underrun and latency qualification, plus controller/wider scratch qualification required by the milestone.
- M3: reviewed EQ/listening and physical microphone/booth/recording/long-session qualification.
- Public release qualification remains a later M9 gate; green development CI alone does not authorize a release.

## Next largest step

Run the exact-head CI for the M2 key-lock listening-witness branch and fix every regression before package integration. If the witness contract is green, stage the script/guide beside the exact Windows development EXE under the existing manifest/checksum contract. Do not advance `docs/progress.json` until real acceptance evidence closes a full milestone.
