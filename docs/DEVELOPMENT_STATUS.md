# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying the already-implemented M1–M4 core workflow before unrelated feature expansion. Optional M5–M8 breadth remains later work unless a concrete Beta blocker requires it.

## Current checkpoint

- Default branch baseline: `main` at `7ea908f58c172a573df527a7c6e85c60278f4c84`, the squash merge of PR #80 (`Harden M4 Windows library witness preflight`). PR #80 was integrated only after exact-head Build and test, Native library scale smoke, M4 witness tool and Beta qualification tool runs all completed successfully.
- Active development: draft PR #81 on `feat/portable-beta-package`. Implementation head `323691340facaa02fca0ecf9f922af8f853b027d` extends the existing package contract to produce a deterministic portable Beta Preview ZIP plus SHA-256 sidecar, validates archive safety/integrity, extracts it into a disposable directory and re-validates the inner exact-source package contract. The packaged Beta Preview guide now describes the portable workflow.
- Local stdlib regression validation for the package-contract implementation passed before push, including deterministic ZIP recreation, payload tamper detection and path-traversal rejection. Exact-head repository CI for PR #81 remains the merge gate; a later checkpoint-only commit is still a distinct PR head and must not be treated as green until its own required runs complete.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. Packaging hardening does not advance M1–M4 or live-readiness evidence.
- GitHub Releases is still empty; no public Beta/Release is authorized.

## Integrated foundations on main

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, a silent device-capability probe and a packaged privacy-safe hardware witness recorder. Real clean-machine playback, device switching and physical master 1/2 versus CUE 3/4 isolation remain manual.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH, the opt-in Signalsmith-backed key-lock research lifecycle and a packaged privacy-safe listening witness recorder. Representative music-domain and real listening/latency/hardware evidence remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, opt-in microphone ducking, limiter measurements, deterministic EQ qualification and a packaged mixer/recording witness. Physical/listening gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore and native 5k-track/12k-history staged-EXE recovery. Its human witness now rejects CI evidence generation, requires Windows 11 x64/x64 PowerShell and preflights the exact staged executable with the no-audio resize/geometry smoke before manual checks.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not make a public Beta Release by itself.
- The main workstation UI has four deck surfaces around a dedicated four-channel center mixer, responsive compact fallback, stronger knob/fader hierarchy, restrained semantic state accents and a deterministic Windows geometry/pixel witness. Hosted CI pixels are regression evidence only; real Windows 11 manual visual review remains open.

## Gates still open before first Beta qualification

- M1: real Windows 11 clean launch/resize/import/playback/device-switch/four-output-cue witness on actual hardware.
- M2: representative music-domain BPM/key evidence, real key-lock listening evidence, device CPU/callback-deadline/underrun and latency qualification, plus controller/wider scratch qualification required by the current milestone wording.
- M3: real Windows 11 reviewed EQ/mixer listening plus physical microphone/ducking/Booth/recording/dropout qualification and long-session evidence required by the witness.
- M4: privacy-safe connected Windows 11 library/session witness from the exact staged executable; the automated preflight is only a prerequisite and cannot close the milestone.
- Packaging: PR #81 must pass exact-head repository CI before its portable ZIP can be treated as the current candidate contract. This does not replace later clean-machine/manual Beta qualification.
- Public Beta publication remains blocked until the applicable qualification evidence, exact candidate CI/package checks, source/notices/checksums and known-issues review are complete. No public release is authorized yet.

## Next largest step

Finish PR #81 exact-head validation and repair any archive/package regression without weakening the checks. Once that portable candidate slice is green and integrated, return to the frozen real Windows 11 M4 connected-library/session witness and then the remaining M1–M3 hardware/listening qualification instead of widening product scope.
