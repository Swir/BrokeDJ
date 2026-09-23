# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope is frozen around completing and qualifying M1–M4. M5+ work remains later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline: `main` at `a3ee4925ac395849dd80169b6e5d9795aff59a10`, the squash merge of PR #87. Exact PR head `9e064ee847abe051db7276f56d4e9335c2d8124f` passed Build and test #443, Native library scale smoke #113 and UI visual witness #21 before integration.
- No feature branch is active after PR #87 integration. Recording hardening is now on `main`: accepted NaN/Inf master samples are serialized as silence without changing timeline length; sanitation versus omission counters remain separately diagnosable; `SetRecorder::capture()` retains its zero-caller-heap contract; and `SetRecorder(fifoFrames)` now preserves the requested usable FIFO capacity across JUCE `AbstractFifo`'s sentinel slot.
- The final Windows regression was a test-fixture expectation bug, not production sample corruption: the shared sanitized-PCM verifier had hard-coded adjacent values at +/-0.15 while the mixed sanitation+overflow fixture used +/-0.20. The verifier now receives each fixture's expected untouched sample levels; production sanitation logic and numeric tolerance were not weakened.
- Recording regressions verify exact zero replacement at corrupted sample positions, unchanged adjacent valid samples, deterministic 4096-frame FIFO overflow accounting, mixed sanitation+overflow reason separation, aggregate/reason counter consistency, late-destination overwrite protection and invalid-start failure.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 remain open until their documented acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, a privacy-safe hardware witness recorder, portable Beta Preview packaging and native four-deck async import/replacement isolation. Real clean-machine import/playback, device switching and physical master 1/2 versus CUE 3/4 isolation remain manual.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle. Representative real-music BPM/key evidence, listening/latency qualification and physical controller workflows remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests. PR #87 is integrated and hardens recorder serialization, reason-specific integrity evidence, the callback heap contract and the requested FIFO-capacity contract; physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and native async session/adoption round-trip coverage. The remaining M4 gate is the documented user-controlled connected-library/session witness on Windows 11 x64.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **M1:** real Windows 11 clean-machine launch/resize/import/playback, device-switch/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** reviewed connected Windows 11 library/session witness against the exact staged executable.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Keep the Beta scope frozen and run/review the privacy-safe M4 connected-library/session witness on Windows 11 x64 against one exact staged executable. Fix any regression it exposes before changing milestone status. Then complete the remaining M1–M3 hardware/listening evidence. Do not widen the target into M5+ merely to keep development busy.
