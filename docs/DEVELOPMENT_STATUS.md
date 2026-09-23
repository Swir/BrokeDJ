# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline is `main` at `98a4caedf0e08d8c53845589874ff5d61621d089`, the squash merge of PR #95. Its exact PR head `c7aa495db580c329d88987f6a7c717c7140275ea` passed Build and test #475, Native library scale smoke #145, UI visual witness #30, M1 hardware witness tool #33, M2 key-lock listening witness tool #28 and M3 mixer/recording witness tool #23 before integration.
- Active package is draft PR #96, branch `fix/m4-witness-state-verification`. Product/test head before this checkpoint is `ab2cb60e6297ab6f4fc6f7ce262fa4896fac9474`; this checkpoint commit creates the newer exact PR head that must receive its own required checks before merge.
- PR #96 hardens M4 file-presence reconciliation. The existing conditional `id/path/old-missing` update already prevented a stale filesystem probe from overwriting a concurrent relocate, but a zero-row guarded update was previously invisible in the refresh summary. The result now reports that raced record as `unresolved` instead of silently presenting the page as fully settled, and removes the stale missing observation from the settled missing count.
- Deterministic coverage now adds explicit UTF-8 path reconciliation, non-regular-path classification/recovery and a zero-page-size clamp test on top of the existing paged move/relocate/tag/playlist retention coverage. Filesystem and SQLite maintenance remain off the audio thread.
- This is software/database hardening only. It does not claim the connected Windows 11 M4 witness has been performed, and it does not replace physical M1–M3 hardware/listening evidence.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 stay open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging, native four-deck async import/replacement isolation, deterministic device-reprepare transport continuity, conservative loss pause/no-auto-resume behavior and retained-but-closed JUCE device detection via `AudioIODevice::isOpen()`. The remaining M1 gate is real Windows 11 clean-machine switching/loss plus physical four-output master/cue isolation.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle. Representative real-music BPM/key evidence, key-lock listening/latency qualification and physical controller workflows remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests. Physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and native async session/adoption round-trip coverage. PR #96 strengthens refresh-summary correctness under a stale conditional update and extends path/recovery tests. The connected Windows 11 witness is still required.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **M1:** real Windows 11 clean-machine launch/resize/import/playback, device-switch/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** exact-head PR #96 CI, then reviewed connected Windows 11 library/session witness against the exact staged executable, including explicit file-status refresh after moving the synthetic fixture and zero unresolved reconciliation results.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Repair every exact-head regression on PR #96 before integration; do not merge while required checks are running or red. Keep the Beta scope frozen. After this M4 refresh-summary hardening is green, the largest remaining product gate is the privacy-safe connected Windows 11 M4 library/session witness, followed by the physical M1 device/four-output checks and remaining M2–M3 listening/hardware evidence. If those manual gates cannot be executed in the current environment, preserve the verified software baseline instead of inventing substitute evidence.
