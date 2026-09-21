# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `bef37252eb97a9bd3d3d465eec1665f083a1c1dc`, the merge commit for PR #72 (`ship self-contained M1 hardware witness`).
- Merged-main verification for that exact commit is green: Build and test `35619027328` and Native library scale smoke `35619027348` both completed successfully.
- GitHub Releases is still empty. There is no qualified public BrokeDJ alpha, beta or stable release.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). M1 and M4 remain open because their documented real Windows 11 manual witnesses have not been performed.
- Active development: draft PR #73, branch `feat/m2-keylock-listening-witness`. Latest functional/package head before this checkpoint documentation commit: `0165aa4749847861e3f98bbeff776f2c8aeea8ee`.
- PR #73 adds a privacy-minimized M2 human listening evidence recorder. It binds evidence to the exact `BrokeDJ.exe`, requires at least three user-owned/licensed representative tracks, requires normal plus slow/fast key-lock listening, transport-fallback, pitch-stability, artifact and runtime-error review, and refuses evidence generation under CI.
- The script/guide are staged beside the Windows development executable, included in the package manifest/checksums and checked by staged-package smoke. CI may validate schema fixtures and prove that the staged tool refuses automated evidence generation; it may never claim that listening occurred.
- The dedicated witness workflow already passed on functional head `8eaabc2bc8983cae0a6e97ad9af02678eda4a7e2` (`35624642882`), including PowerShell parsing and negative cases for incomplete/spoofed/privacy-unsafe/wrong-identity evidence plus CI-generation refusal.
- Package head `0165aa47…` queued Build and test `35625068809`, Native library scale smoke `35625068837`, M2 key-lock listening witness tool `35625068845`, and M1 hardware witness tool `35625068833`. No result is assumed until GitHub reports completion.
- This checkpoint commit changes the PR head. Fresh exact-final-head checks remain mandatory before any merge even if the package-head runs finish green.
- Production key lock remains blocked on real representative-material listening plus device CPU/callback-deadline/underrun and latency evidence. No milestone progress was advanced for tooling.

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

Let the fresh exact-final-head PR #73 checks complete and fix every regression before merge. If all required checks are green on the same head, the branch is eligible for integration as tooling/blocker-removal only; `docs/progress.json` must remain unchanged until real acceptance evidence closes a full milestone.
