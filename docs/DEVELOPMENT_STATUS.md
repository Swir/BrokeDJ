# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope is frozen around completing and qualifying M1–M4. M5+ work remains later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline: `main` at `5c944e5bcb2c3cb0c84d241bdaf7d68d1e89f716`, the squash merge of PR #86. Its exact PR head passed Build and test #434, Native library scale smoke #104, M4 witness tool #23 and Beta qualification tool #15 before integration. The merged M4 witness rejects unattended CI evidence generation before touching the supplied executable and publishes evidence only after complete closed-schema validation.
- Active development branch: `fix/recording-nonfinite-boundary`, draft PR #87. Recorder FIFO regression fix commit: `d8135be37103a3c0935ad5eeb9e2ecda8c8c8146`; this documentation checkpoint is the next branch head and must receive its own exact-head CI before integration.
- Exact-head run #439 on the preceding checkpoint `2e53a96801a7228dfbc0827acdb1be5181b0b952` built the Windows x64 application successfully and passed 34/35 CTest targets; only `set_recorder` failed. Native library scale #109 and UI visual witness #17 passed. The failure exposed a real capacity-contract bug rather than a compiler or packaging failure.
- JUCE 9.0.2 `AbstractFifo` documents that usable capacity is one less than its total buffer size. `SetRecorder(fifoFrames)` previously allocated exactly `fifoFrames` storage slots, silently making the real SPSC capacity one frame smaller than the requested contract. The fix allocates the required sentinel slot explicitly, preserves `fifoFrames` as the usable capacity, and guards integer overflow. The deterministic 4096-frame overflow fixture now tests the intended production contract instead of depending on an accidental off-by-one.
- The recorder PCM boundary sanitizes accepted NaN/Inf samples to silence without changing timeline length and preserves aggregate affected-frame/dropout counters. Separate sanitation-versus-omission frame/event totals distinguish corrupted-but-preserved timeline positions from frames that could not enter the FIFO/input path.
- Recording regressions verify exact zero replacement at corrupted sample positions, unchanged adjacent valid samples, deterministic 4096-frame FIFO overflow accounting, mixed sanitation+overflow reason separation, aggregate/reason counter consistency, late-destination overwrite protection and invalid-start failure.
- The `SetRecorder::capture()` test measures the caller/audio-thread heap contract over 64 x 256-frame blocks and requires zero caller-thread allocation/deallocation. Its `ns/frame` value is diagnostic only and is not treated as a shared-runner hardware deadline threshold.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 remain open until their documented acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, a privacy-safe hardware witness recorder, portable Beta Preview packaging and native four-deck async import/replacement isolation. Real clean-machine import/playback, device switching and physical master 1/2 versus CUE 3/4 isolation remain manual.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle. Representative real-music BPM/key evidence, listening/latency qualification and physical controller workflows remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests. PR #87 hardens recorder serialization, reason-specific integrity evidence, the callback heap contract and the requested FIFO-capacity contract; physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and native async session/adoption round-trip coverage. The remaining M4 gate is the documented user-controlled connected-library/session witness on Windows 11 x64.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **M1:** real Windows 11 clean-machine launch/resize/import/playback, device-switch/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** reviewed connected Windows 11 library/session witness against the exact staged executable.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Run the full exact-head gate for the final PR #87 checkpoint and repair any remaining regression before integration. If Build/Test, native 5k library scale, staged package and UI witness remain green, keep the Beta scope frozen and integrate only in the normal coherent merge window. Then run/review the privacy-safe M4 connected-library/session witness on Windows 11 x64, followed by the remaining M1–M3 hardware/listening evidence. Do not widen the target into M5+ merely to keep development busy.
