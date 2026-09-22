# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying the already-implemented M1–M4 core workflow before unrelated feature expansion. Optional M5–M8 breadth remains later work unless a concrete Beta blocker requires it.

## Current checkpoint

- Default branch baseline: `main` at `912fc8e82c9d89adbd404a8053674a0f6130b1c7`, the squash merge of PR #82 (`Harden four-deck native async import smoke`). Its exact head passed Build and test run #421 and Native library scale smoke run #91 before integration.
- Active development: draft PR #83 on `feat/native-session-restore-smoke`. Implementation head `94915825ea108b7b3b9c551ded03d6476ed96729` extends the existing no-device native import smoke into a four-deck session lifecycle round trip through the real async decoder, `Engine` adoption mailbox and empty-slot eject path.
- PR #83 also validates atomic session save/checksum load and explicit verified `.bak` recovery after a deliberately damaged primary snapshot. Restored non-default mixer/deck controls and paused seek positions are checked only after audio-side adoption; a persisted `wasPlaying=true` is required to restore paused.
- The integration render is offline into a temporary buffer. It opens no physical device, plays no audible output and uses only original synthetic WAV fixtures in a temporary directory.
- Exact-head qualification for implementation head `94915825...` started as Build and test run #423 and Native library scale smoke run #93. Both were queued when this checkpoint was written; a later metadata-only checkpoint commit may trigger superseding exact-head runs, which must be used for merge decisions.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. The stronger session automation does not replace the connected Windows 11 witness and does not close M4.
- GitHub Releases is still empty; no public Beta/Release is authorized.

## Integrated foundations on main

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, a silent device-capability probe, a packaged privacy-safe hardware witness recorder, a deterministic portable Beta Preview candidate and native four-deck async import/replacement isolation. Real clean-machine import/playback, device switching and physical master 1/2 versus CUE 3/4 isolation remain manual.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH, the opt-in Signalsmith-backed key-lock research lifecycle and a packaged privacy-safe listening witness recorder. Representative music-domain and real listening/latency/hardware evidence remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, opt-in microphone ducking, limiter measurements, deterministic EQ qualification and a packaged mixer/recording witness. Physical/listening gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore and native 5k-track/12k-history staged-EXE recovery. Its human witness rejects CI evidence generation, requires Windows 11 x64/x64 PowerShell and preflights the exact staged executable with the no-audio resize/geometry smoke before manual checks.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not make a public Beta Release by itself.
- The main workstation UI has four deck surfaces around a dedicated four-channel center mixer, responsive compact fallback, stronger knob/fader hierarchy, restrained semantic state accents and a deterministic Windows geometry/pixel witness. Hosted CI pixels are regression evidence only; real Windows 11 manual visual review remains open.

## Gates still open before first Beta qualification

- M1: real Windows 11 clean launch/resize/import/playback/device-switch/four-output-cue witness on actual hardware.
- M2: representative music-domain BPM/key evidence, real key-lock listening evidence, device CPU/callback-deadline/underrun and latency qualification, plus controller/wider scratch qualification required by the current milestone wording.
- M3: real Windows 11 reviewed EQ/mixer listening plus physical microphone/ducking/Booth/recording/dropout qualification and long-session evidence required by the witness.
- M4: privacy-safe connected Windows 11 library/session witness from the exact staged executable. PR #83 materially reduces native session-regression risk but remains automated prerequisite evidence only.
- Public Beta publication remains blocked until the applicable qualification evidence, exact candidate CI/package checks, source/notices/checksums and known-issues review are complete. No public release is authorized yet.

## Next largest step

Qualify PR #83 on its final exact head. Fix any real native session regression without weakening the checks; do not merge while required CI is red or running. Once the accumulated package is exact-head green, keep the M4 milestone open until the documented Windows 11 connected-library/session witness is genuinely performed and reviewed, then continue the remaining frozen M1–M3 hardware/listening qualification rather than widening product scope.
