# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying the already-implemented M1–M4 core workflow before unrelated feature expansion. Optional M5–M8 breadth remains later work unless a concrete Beta blocker requires it.

## Current checkpoint

- Default branch baseline: `main` at `07647cbcabb8a64c0112a202fb8cac9076c5bde3`, the merge of PR #81 (`Package deterministic portable Beta Preview candidate`). PR #81 integrated only after its exact-head Build and test plus Native library scale smoke both completed successfully. The portable candidate contract now lives on `main`; GitHub Releases remains empty.
- Active development: draft PR #82 on `feat/native-import-smoke`. The branch adds `native_import_smoke`, a Windows-native CTest around the real `MainComponent` asynchronous load boundary with audio-device initialization disabled. The fixture is original synthetic stereo WAV data created in a temporary directory; the test verifies source publication into deck/session state, rejects a concurrent replacement while loading and then proves a deliberately invalid replacement preserves the previous working source.
- This is stronger automated M1 import-regression evidence, but it does not open hardware, play audio or substitute for real Windows 11 file-picker/drag-drop/manual playback qualification. Exact-head PR #82 CI is the merge gate; do not treat the branch as integrated while checks are missing, running or red.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. An automated import smoke does not close M1 or change live-readiness.
- GitHub Releases is still empty; no public Beta/Release is authorized.

## Integrated foundations on main

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, a silent device-capability probe, a packaged privacy-safe hardware witness recorder and a deterministic portable Beta Preview candidate. Real clean-machine import/playback, device switching and physical master 1/2 versus CUE 3/4 isolation remain manual.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH, the opt-in Signalsmith-backed key-lock research lifecycle and a packaged privacy-safe listening witness recorder. Representative music-domain and real listening/latency/hardware evidence remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, opt-in microphone ducking, limiter measurements, deterministic EQ qualification and a packaged mixer/recording witness. Physical/listening gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore and native 5k-track/12k-history staged-EXE recovery. Its human witness rejects CI evidence generation, requires Windows 11 x64/x64 PowerShell and preflights the exact staged executable with the no-audio resize/geometry smoke before manual checks.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not make a public Beta Release by itself.
- The main workstation UI has four deck surfaces around a dedicated four-channel center mixer, responsive compact fallback, stronger knob/fader hierarchy, restrained semantic state accents and a deterministic Windows geometry/pixel witness. Hosted CI pixels are regression evidence only; real Windows 11 manual visual review remains open.

## Gates still open before first Beta qualification

- M1: real Windows 11 clean launch/resize/import/playback/device-switch/four-output-cue witness on actual hardware. PR #82 only strengthens the automated async-import regression boundary.
- M2: representative music-domain BPM/key evidence, real key-lock listening evidence, device CPU/callback-deadline/underrun and latency qualification, plus controller/wider scratch qualification required by the current milestone wording.
- M3: real Windows 11 reviewed EQ/mixer listening plus physical microphone/ducking/Booth/recording/dropout qualification and long-session evidence required by the witness.
- M4: privacy-safe connected Windows 11 library/session witness from the exact staged executable; the automated preflight is only a prerequisite and cannot close the milestone.
- Public Beta publication remains blocked until the applicable qualification evidence, exact candidate CI/package checks, source/notices/checksums and known-issues review are complete. No public release is authorized yet.

## Next largest step

Finish PR #82 exact-head Windows validation and repair any real native import regression without weakening the checks. Once it is green and integrated, return immediately to the frozen real Windows 11 M4 connected-library/session witness and then the remaining M1–M3 hardware/listening qualification instead of widening product scope.
