# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Current checkpoint

- Default-branch product baseline: PR #93 was squash-merged to `main` as `36223aa2e13fed8da0ed9b29182fad36db103490` from exact head `56fd67420c3e3c4835325ebc69dea60a6e96f743`. The exact PR head passed Build and test #465, Native library scale smoke #135, M1 hardware witness tool #28, M2 key-lock listening witness tool #23 and M3 mixer/recording witness tool #18 before integration.
- PR #93 fixes a real M1 engine-side device lifecycle regression: a stopped-audio `Engine::prepare()` no longer rewinds loaded deck source-frame transport or collapses a split Reverse/Slip audible cursor when an output device or sample rate is re-prepared. Device-rate resampler tables, scratch storage, filters, echo history and smoothers are still rebuilt for the new configuration.
- Deterministic core coverage performs a 48 kHz -> 44.1 kHz reprepare during forward master/cue playback and a 48 kHz -> 96 kHz reprepare during split Reverse+Slip. It verifies preserved play intent, bounded continuous transport, finite master/cue output, continued hidden forward transport and continued audible reverse ownership.
- The Windows development build and staged-artifact jobs for PR #93 passed. These checks are software lifecycle evidence only; they do not claim a physical Windows audio-device switch, device-loss recovery, real four-output cue isolation, controller timing or reviewed listening.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 stay open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging, native four-deck async import/replacement isolation and deterministic device-reprepare transport continuity. The remaining M1 gate is real Windows 11 clean-machine switching/loss plus physical four-output master/cue isolation.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle. Representative real-music BPM/key evidence, key-lock listening/latency qualification and physical controller workflows remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests. Physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and native async session/adoption round-trip coverage. PR #92 added bounded off-thread file-presence reconciliation plus an explicit Library **Refresh file status** workflow. The connected Windows 11 witness is still required.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **M1:** real Windows 11 clean-machine launch/resize/import/playback, device-switch/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** reviewed connected Windows 11 library/session witness against the exact staged executable, including explicit file-status refresh after moving the synthetic fixture.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Keep the Beta scope frozen. The largest remaining product gate is the privacy-safe M4 connected-library/session witness on Windows 11 x64 using the integrated disposable fixture workspace and explicit file-status refresh. After M4 review, finish the real M1 device/four-output checks and remaining physical M2–M3 listening/hardware evidence. Do not widen the target into M5+ merely to keep development busy; if the manual gates cannot be executed in the current environment, preserve the verified software baseline instead of inventing substitute evidence.
