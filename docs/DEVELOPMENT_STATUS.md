# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying the already-implemented M1–M4 core workflow before unrelated feature expansion. Optional M5–M8 breadth remains later work unless a concrete Beta blocker requires it.

## Current checkpoint

- Default branch baseline: `main` at `7d9e55aebb91066f3d3a2e0db9a1e2ffaad00879`, the squash merge of PR #84. Its exact PR head `7d53345b258b11537a0c9452a49e122a06f488fb` passed Build and test #428, Native library scale smoke #98, M1 hardware witness tool #21 and Beta qualification tool #13 before integration. The merged guard prevents unattended CI from generating human M1 hardware evidence while leaving validation of existing evidence CI-usable.
- Active development branch: `feat/constant-time-deck-adoption`, draft PR #85. Implementation commit `4d05293894d6e505048816927afe7d6142acba86` removes the buffer-length-dependent full echo-ring clear from clip adoption/eject handling. Adoption now invalidates the prepared 250 ms echo history in O(1); old ring slots are treated as silence until each slot has been overwritten once by the new deck epoch.
- PR #85 adds a deterministic 192 kHz quality regression with a fully populated 48,000-frame-per-channel echo ring. It replaces the source with silence, verifies that no stale previous-deck echo reaches the new source, and records the adoption callback elapsed time as diagnostic-only evidence rather than a shared-runner performance threshold.
- No decoder, device-open, routing, GUI, library schema or roadmap acceptance criterion changes in PR #85. The work addresses the explicit realtime callback spike noted during the Beta hardening pass; physical callback-deadline/underrun and listening evidence remain separate.
- Exact-head CI for PR #85 is required before integration. Until the final PR head is green, this package stays on the development branch and must not be reported as merged or Beta-ready.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 remain open until their documented acceptance evidence exists.
- GitHub Releases is still empty; no public Beta/Release is authorized.

## Integrated foundations on main

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, a silent device-capability probe, a packaged privacy-safe hardware witness recorder, a deterministic portable Beta Preview candidate and native four-deck async import/replacement isolation. Real clean-machine import/playback, device switching and physical master 1/2 versus CUE 3/4 isolation remain manual.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH, the opt-in Signalsmith-backed key-lock research lifecycle and a packaged privacy-safe listening witness recorder. Representative music-domain and real listening/latency/hardware evidence remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, opt-in microphone ducking, limiter measurements, deterministic EQ qualification and a packaged mixer/recording witness. Physical/listening gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and the native async session/adoption round trip integrated in PR #83. Its human witness rejects CI evidence generation, requires Windows 11 x64/x64 PowerShell and preflights the exact staged executable with the no-audio resize/geometry smoke before manual checks.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not make a public Beta Release by itself.
- The main workstation UI has four deck surfaces around a dedicated four-channel center mixer, responsive compact fallback, stronger knob/fader hierarchy, restrained semantic state accents and deterministic Windows geometry/pixel witnesses. Hosted CI pixels are regression evidence only; real Windows 11 manual visual review remains open.

## Gates still open before first Beta qualification

- M1: real Windows 11 clean launch/resize/import/playback/device-switch/four-output-cue witness on actual hardware.
- M2: representative music-domain BPM/key evidence, real key-lock listening evidence, device CPU/callback-deadline/underrun and latency qualification, plus controller/wider scratch qualification required by the current milestone wording.
- M3: real Windows 11 reviewed EQ/mixer listening plus physical microphone/ducking/Booth/recording/dropout qualification and long-session evidence required by the witness.
- M4: privacy-safe connected Windows 11 library/session witness from the exact staged executable.
- Public Beta publication remains blocked until the applicable qualification evidence, exact candidate CI/package checks, source/notices/checksums and known-issues review are complete. No public release is authorized yet.

## Next largest step

Finish exact-head CI for draft PR #85 and fix any regression it exposes; do not merge while required checks are running or red. If the package stays green, keep the current Beta scope frozen and integrate it only at the normal coherent merge window. Then run and review the documented privacy-safe M4 connected-library/session witness on Windows 11 x64 against one exact staged executable, followed by the remaining M1–M3 hardware/listening evidence. Do not widen the target into M5+ feature work merely to keep development busy.
