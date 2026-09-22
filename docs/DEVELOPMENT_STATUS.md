# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying the already-implemented M1–M4 core workflow before unrelated feature expansion. Optional M5–M8 breadth remains later work unless a concrete Beta blocker requires it.

## Current checkpoint

- Default branch baseline: `main` at `ca0fdae29fae7297a3c1eba103ab1c9d9a81b0d4`, the squash merge of PR #78 (`Harden Windows UI witness and workstation geometry`). PR #78 was integrated only after its exact-head Windows Build and test, native library-scale smoke and UI visual witness were green; both compact/workstation PNGs plus the pixel-quality report were reviewed. Issue #79 is closed because the full 22-button deck geometry, eight Hot Cues and real grid actions are now covered by the native containment/overlap smoke.
- Active development: draft PR #80 on `feat/m4-witness-preflight`. Product/tool implementation head `aa0bab80de1b53aec0d087136dc027868c26fb1c` strengthens the remaining M4 manual-witness path without changing DSP, routing, decoder, library schema or realtime ownership. Human evidence generation now fails closed in CI, Windows 11 x64/x64 PowerShell is required before prompts, and the exact selected `BrokeDJ.exe` must pass its no-audio `--smoke-test` resize/geometry contract in a disposable temporary directory before the eight manual M4 checks begin.
- PR #80 CI had not yet published workflow runs when this checkpoint was written. The PR must remain unmerged until the exact final PR head is green; the automated preflight is not permission to mark M4 complete without the real connected Windows 11 library/session witness.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. UI hardening and witness-tool integrity do not advance the milestone counter.
- GitHub Releases is still empty; no public Beta/Release is authorized.

## Integrated foundations on main

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, a silent device-capability probe and a packaged privacy-safe hardware witness recorder. Real clean-machine playback, device switching and physical master 1/2 versus CUE 3/4 isolation remain manual.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH, the opt-in Signalsmith-backed key-lock research lifecycle and a packaged privacy-safe listening witness recorder. Representative music-domain and real listening/latency/hardware evidence remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, opt-in microphone ducking, limiter measurements, deterministic EQ qualification and a packaged mixer/recording witness. Physical/listening gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe backup/restore and native 5k-track/12k-history staged-EXE recovery.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not make a public Beta Release by itself.
- The main workstation UI has four deck surfaces around a dedicated four-channel center mixer, responsive compact fallback, stronger knob/fader hierarchy, restrained semantic state accents and a deterministic Windows geometry/pixel witness. Hosted CI pixels are regression evidence only; real Windows 11 manual visual review remains open.

## Gates still open before first Beta qualification

- M1: real Windows 11 clean launch/resize/import/playback/device-switch/four-output-cue witness on actual hardware.
- M2: representative music-domain BPM/key evidence, real key-lock listening evidence, device CPU/callback-deadline/underrun and latency qualification, plus controller/wider scratch qualification required by the current milestone wording.
- M3: real Windows 11 reviewed EQ/mixer listening plus physical microphone/ducking/Booth/recording/dropout qualification and long-session evidence required by the witness.
- M4: PR #80 must first pass exact-head CI. Then the privacy-safe connected Windows 11 library/session witness must be run against the exact staged executable and reviewed; its new automatic no-audio preflight is only a prerequisite, not completion evidence.
- Public Beta publication remains blocked until the applicable qualification evidence, exact candidate CI/package checks, source/notices/checksums and known-issues review are complete. No public release is authorized yet.

## Next largest step

Finish PR #80 exact-head validation and fix any real witness-tool regression rather than weakening the checks. Once it is green, integrate the package and return to the real Windows 11 M4 connected-library/session witness; after M4 evidence is genuinely accepted, continue the frozen M1–M3 hardware/listening qualification instead of widening product scope.
