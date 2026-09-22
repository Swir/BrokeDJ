# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying the already-implemented M1–M4 core workflow before unrelated feature expansion. Optional M5–M8 breadth remains later work unless a concrete Beta blocker requires it.

## Current checkpoint

- Default branch baseline: `main` at `a1d9e988784066c9ed1d3b498c9a543f614cde87`, the squash merge of PR #77 (`Improve Beta UI control hierarchy and legibility`). PR #77 was integrated after its exact-head Windows Build and test, native library-scale smoke and UI visual witness were green and the compact/workstation PNGs were reviewed. The merge changes presentation only; it does not change Engine/DSP/routing/device/decoder ownership.
- Active development: draft PR #78 on `feat/ui-visual-quality-gate`. Implementation head `233e967a0760d8950c7c1b3a50f852628332d014` adds a deterministic pixel sanity gate for the existing Windows compact/workstation UI witness and documents its qualification boundary. UI visual witness run `35739447333` was queued for that implementation head when this checkpoint was prepared; a later documentation checkpoint must not be mistaken for successful exact-final-head validation.
- The new gate measures broad luminance, luminance variation, bright-detail fraction, BrokeDJ blue/cyan accent presence, quantized colour diversity and painted-content fraction. It is intended to reject blank, corrupt, flat or grossly palette-regressed captures that can still have plausible dimensions/file size. Hosted-runner off-screen black pixels are explicitly tolerated within a bounded envelope rather than misreported as an application failure.
- This gate analyzes already-captured PNG files only. It does not open audio, touch devices, change the realtime callback, score aesthetics, certify accessibility/HiDPI usability or replace human Windows 11 review.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. UI integration and witness hardening do not advance the milestone counter.
- GitHub Releases is still empty; no public Beta/Release is authorized.

## Integrated foundations on main

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, a silent device-capability probe and a packaged privacy-safe hardware witness recorder. Real clean-machine playback, device switching and physical master 1/2 versus CUE 3/4 isolation remain manual.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH, the opt-in Signalsmith-backed key-lock research lifecycle and a packaged privacy-safe listening witness recorder. Representative music-domain and real listening/latency/hardware evidence remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, opt-in microphone ducking, limiter measurements, deterministic EQ qualification and a packaged mixer/recording witness. Physical/listening gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe backup/restore and native 5k-track/12k-history staged-EXE recovery.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not make a public Beta Release by itself.
- The main workstation UI now has four deck surfaces around a dedicated four-channel center mixer, responsive compact fallback, stronger knob/fader hierarchy and restrained semantic state accents. Hosted CI pixels are regression evidence only; real Windows 11 manual visual review remains open.

## Gates still open before first Beta qualification

- M1: real Windows 11 clean launch/resize/import/playback/device-switch/four-output-cue witness on actual hardware.
- M2: representative music-domain BPM/key evidence, real key-lock listening evidence, device CPU/callback-deadline/underrun and latency qualification, plus controller/wider scratch qualification required by the current milestone wording.
- M3: real Windows 11 reviewed EQ/mixer listening plus physical microphone/ducking/Booth/recording/dropout qualification and long-session evidence required by the witness.
- M4: real Windows 11 connected-library/session witness from the exact staged executable.
- UI: PR #78 must pass its exact-final-head native Windows witness with the new pixel-quality report, and both PNGs plus the JSON metrics must be reviewed. Real Windows 11 HiDPI/usability remains separate.
- Public Beta publication remains blocked until the applicable qualification evidence, exact candidate CI/package checks, source/notices/checksums and known-issues review are complete. No public release is authorized yet.

## Next largest step

Finish PR #78 exact-final-head Windows UI witness, inspect its compact/workstation PNGs and pixel-quality report, and fix the gate rather than weakening thresholds if it exposes a real capture regression. Once coherent, integrate this Beta qualification slice; then return to the frozen M1–M4 real Windows hardware/listening/library-session witnesses instead of widening product scope.
