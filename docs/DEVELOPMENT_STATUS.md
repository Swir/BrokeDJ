# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying the already-implemented M1–M4 core workflow before unrelated feature expansion. Optional M5–M8 breadth remains later work unless a concrete Beta blocker requires it.

## Current checkpoint

- Default branch baseline: `main` at `8026efc16773a5ad81dd6988ce13de0e6b75f901`. Its post-merge exact-head Build and test run #426 and Native library scale smoke run #96 both completed successfully.
- Active development branch: `feat/m1-ci-evidence-guard`, draft PR #84. The package closes an evidence-integrity gap in the M1 human hardware witness: generation now fails before any Windows/device interaction when `CI` or `GITHUB_ACTIONS` is truthy, including common case/boolean forms (`true`, `TRUE`, `1`, `yes`, `ON`). `-ValidateExisting` remains CI-usable.
- PR #84 also extends the dedicated M1 witness-tool workflow with negative generation tests that require the failure to come specifically from the CI guard and verify that no M1 evidence JSON is written. The initial code head was `0eab89dc44ad1cd03594ec37280792c50162315e`; exact-head workflows were queued when this checkpoint was written and must be green on the final PR head before integration.
- No audio DSP, GUI, routing, device-open behavior or roadmap acceptance criterion changes in PR #84. The package hardens trust in future manual evidence; it does not itself satisfy the physical M1 witness.
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

- M1: real Windows 11 clean launch/resize/import/playback/device-switch/four-output-cue witness on actual hardware. PR #84 must also pass exact-head CI before its evidence-integrity hardening can be integrated.
- M2: representative music-domain BPM/key evidence, real key-lock listening evidence, device CPU/callback-deadline/underrun and latency qualification, plus controller/wider scratch qualification required by the current milestone wording.
- M3: real Windows 11 reviewed EQ/mixer listening plus physical microphone/ducking/Booth/recording/dropout qualification and long-session evidence required by the witness.
- M4: privacy-safe connected Windows 11 library/session witness from the exact staged executable.
- Public Beta publication remains blocked until the applicable qualification evidence, exact candidate CI/package checks, source/notices/checksums and known-issues review are complete. No public release is authorized yet.

## Next largest step

Finish exact-head CI for draft PR #84 and fix any regression it exposes; do not merge while required checks are running or red. Because BrokeDJ default-branch integration is intentionally throttled to coherent 4–6 hour packages, keep a green PR #84 on the development branch unless an earlier merge is justified by a real urgent integrity regression. After that, run and review the documented privacy-safe M4 connected-library/session witness on Windows 11 x64 against one exact staged executable, then complete the frozen M1–M3 hardware/listening evidence. Do not widen the current Beta target into M5+ feature work merely to keep the automation busy.
