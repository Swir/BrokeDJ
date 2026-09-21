# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope is frozen around completing and qualifying the already-implemented M1–M4 core workflow before unrelated feature expansion. Optional M5–M8 breadth remains later work unless a concrete Beta blocker requires it.

## Current checkpoint

- Default branch baseline: `main` at `c33565a4b95686fac760966e60948580206d42c5`, merge commit for PR #74 (`package M3 mixer and recording witness`).
- Merged-main verification for `c33565a4…` is green: Build and test `35631952797` and Native library scale smoke `35631952804` both completed successfully.
- No open issue or competing PR was present when this run started; GitHub Releases remains empty.
- Active development: draft PR #75, branch `feat/beta-qualification-gate`.
- Functional/package head before this checkpoint documentation commit: `94f759a2b1cd94c2116017c268428d72429a70b9`.
- PR #75 adds `scripts/beta_qualification.ps1`, a human-controlled Windows 11 x64 orchestrator that re-runs the existing M1–M4 witness validators against one exact `BrokeDJ.exe` before it can create a Beta qualification summary.
- The summary is bound to `SOURCE-COMMIT.txt`, the executable SHA-256, the M1 full device-probe SHA-256 and each M1–M4 evidence SHA-256. It stores no device names, track names/paths, recording paths, source music or microphone audio.
- Generation refuses CI. Existing summaries can be revalidated and fail closed when the executable, source commit, probe, witness files, gate booleans, hashes, schema or privacy contract changes.
- Windows staging now includes `BETA-QUALIFICATION.ps1` plus `BETA-QUALIFICATION.md`; the downloaded-package smoke requires both and verifies that CI cannot mint a human Beta qualification file.
- The checkpoint commit changes the PR head, so fresh exact-final-head Build/Test and scale-smoke results remain mandatory before any integration.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). The Beta gate tooling does not advance that counter by itself.

## Integrated foundations on main

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, a silent device-capability probe and a packaged privacy-safe hardware witness recorder. Real clean-machine playback, device switching and physical master 1/2 versus CUE 3/4 isolation remain manual.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH, the opt-in Signalsmith-backed key-lock research lifecycle and a packaged privacy-safe listening witness recorder. Representative music-domain and real listening/latency/hardware evidence remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, opt-in microphone ducking, limiter measurements, deterministic EQ qualification and a packaged mixer/recording witness. Physical/listening gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe backup/restore and native 5k-track/12k-history staged-EXE recovery.

## Gates still open before first Beta qualification

- M1: real Windows 11 clean launch/resize/import/playback/device-switch/four-output-cue witness on actual hardware.
- M2: representative music-domain BPM/key evidence, real key-lock listening evidence, device CPU/callback-deadline/underrun and latency qualification, plus controller/wider scratch qualification required by the current milestone wording.
- M3: real Windows 11 reviewed EQ/mixer listening plus physical microphone/ducking/Booth/recording/dropout qualification and long-session evidence required by the witness.
- M4: real Windows 11 connected-library/session witness from the exact staged executable.
- After all four evidence files validate against one exact staged executable, PR #75's Beta orchestrator can produce one privacy-safe qualification summary for that candidate. Public Beta publication still requires exact-head CI, packaged EXE smoke, source/notices/checksums and known-issues review; no public release is authorized yet.

## Next largest step

Let exact-final-head CI for PR #75 run and fix every regression before integration. Once the orchestrator package is green, the remaining Beta blockers are the real Windows 11 M1–M4 manual/hardware/listening workflows above rather than more same-version feature expansion.
