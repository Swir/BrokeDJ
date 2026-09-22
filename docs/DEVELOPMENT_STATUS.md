# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying the already-implemented M1–M4 core workflow before unrelated feature expansion. Optional M5–M8 breadth remains later work unless a concrete Beta blocker requires it.

## Current checkpoint

- Default branch baseline: `main` at `f9c78cf2a76a7d57403239748e9cdab2e0afdd46`, the squash merge of PR #76 (`Polish the native DJ workstation UI`). PR #76 was integrated only after exact-head Build and test, native library-scale, UI visual witness and M1/M2/M3/Beta qualification workflows were green and both Windows UI witness captures were reviewed.
- Active development: draft PR #77 on `feat/beta-ui-legibility-pass`. Its implementation checkpoint is `2ea666dc4eaf3c662510c1e67afc314f0b8ce32d`.
- The follow-up is presentation-only: larger/more readable rotary and fader treatment, stronger disabled-state contrast and restrained semantic state accents for existing transport/mixer controls. It does not change Engine callbacks, routing, device lifecycle, recording, decoding, workers or DSP.
- Initial implementation-head workflows were queued as Build and test `35736050343`, Native library scale smoke `35736050243` and UI visual witness `35736050206`. This status checkpoint follows that implementation commit, so those runs are evidence for the implementation snapshot only; fresh exact-final-head validation is required before merge.
- The fresh Windows visual witness must again capture and review both the workstation 1440×960-class layout and compact 1050×800 layout. Hosted CI screenshots still do not certify real Windows 11 HiDPI quality, manual usability or live performance.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. The UI pass does not advance the milestone counter.
- GitHub Releases remains empty; no public Beta/Release is authorized.

## Integrated foundations on main

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, a silent device-capability probe and a packaged privacy-safe hardware witness recorder. Real clean-machine playback, device switching and physical master 1/2 versus CUE 3/4 isolation remain manual.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH, the opt-in Signalsmith-backed key-lock research lifecycle and a packaged privacy-safe listening witness recorder. Representative music-domain and real listening/latency/hardware evidence remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, opt-in microphone ducking, limiter measurements, deterministic EQ qualification and a packaged mixer/recording witness. Physical/listening gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe backup/restore and native 5k-track/12k-history staged-EXE recovery.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not make a public Beta Release by itself.

## Gates still open before first Beta qualification

- M1: real Windows 11 clean launch/resize/import/playback/device-switch/four-output-cue witness on actual hardware.
- M2: representative music-domain BPM/key evidence, real key-lock listening evidence, device CPU/callback-deadline/underrun and latency qualification, plus controller/wider scratch qualification required by the current milestone wording.
- M3: real Windows 11 reviewed EQ/mixer listening plus physical microphone/ducking/Booth/recording/dropout qualification and long-session evidence required by the witness.
- M4: real Windows 11 connected-library/session witness from the exact staged executable.
- UI: PR #77 requires exact-final-head build/package/scale checks plus a fresh Windows UI pixel witness and review. Real Windows 11 HiDPI/usability remains separate.
- Public Beta publication remains blocked until the applicable qualification evidence, exact candidate CI/package checks, source/notices/checksums and known-issues review are complete. No public release is authorized yet.

## Next largest step

Finish exact-final-head CI for PR #77 and inspect its workstation/compact Windows witness. Fix any clipping, state-contrast or control-density regression in the same PR. After this Beta usability slice is coherent, return to the frozen M1–M4 real Windows hardware/listening/library-session witness gates instead of widening feature scope.
