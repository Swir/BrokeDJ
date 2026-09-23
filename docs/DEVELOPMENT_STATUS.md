# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope is frozen around completing and qualifying M1–M4. M5+ work remains later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline: `main` at `5c944e5bcb2c3cb0c84d241bdaf7d68d1e89f716`, the squash merge of PR #86. Its exact PR head passed Build and test #434, Native library scale smoke #104, M4 witness tool #23 and Beta qualification tool #15 before integration. The merged M4 witness rejects unattended CI evidence generation before touching the supplied executable and publishes evidence only after complete closed-schema validation.
- Active development branch: `fix/recording-nonfinite-boundary`, draft PR #87. Current implementation commit before this documentation checkpoint is `fbcd676bd1037756502227bbe9a94067a666c5a9`.
- The recorder PCM boundary now sanitizes accepted NaN/Inf samples to silence without changing timeline length and preserves the existing aggregate affected-frame/dropout counters. It additionally records separate sanitation-versus-omission frame/event totals, so corrupted-but-preserved timeline positions are distinguishable from frames that could not enter the FIFO/input path.
- Recording regressions now verify exact zero replacement at corrupted sample positions, unchanged adjacent valid samples, deterministic 4096-frame FIFO overflow accounting, mixed sanitation+overflow reason separation and aggregate/reason counter consistency.
- The `SetRecorder::capture()` test now measures the caller/audio-thread heap contract over 64 x 256-frame blocks and requires zero caller-thread allocation/deallocation. Its `ns/frame` value is diagnostic only and is not treated as a shared-runner hardware deadline threshold.
- Initial exact-head runs for implementation commit `fbcd676...`: Build and test #438, Native library scale smoke #108 and UI visual witness #16. They were running when this checkpoint was prepared. The documentation checkpoint becomes a newer PR head, so all required checks must be re-evaluated for that final head before integration.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 remain open until their documented acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, a privacy-safe hardware witness recorder, portable Beta Preview packaging and native four-deck async import/replacement isolation. Real clean-machine import/playback, device switching and physical master 1/2 versus CUE 3/4 isolation remain manual.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle. Representative real-music BPM/key evidence, listening/latency qualification and physical controller workflows remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests. PR #87 hardens recorder serialization, diagnostic reason evidence and the capture callback heap contract; physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and native async session/adoption round-trip coverage. The remaining M4 gate is the documented user-controlled connected-library/session witness on Windows 11 x64.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **M1:** real Windows 11 clean-machine launch/resize/import/playback, device-switch/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** reviewed connected Windows 11 library/session witness against the exact staged executable.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Finish exact-head CI for the final draft PR #87 head and repair any regression before integration. If the recording-boundary package remains green, keep the Beta scope frozen and leave integration to the normal coherent merge window; then run/review the privacy-safe M4 connected-library/session witness on Windows 11 x64, followed by the remaining M1–M3 hardware/listening evidence. Do not widen the target into M5+ merely to keep development busy.
