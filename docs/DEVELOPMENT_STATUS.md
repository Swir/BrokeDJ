# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline: `main` at `3dbc546bd516ec03d72d23033fd64c98a025203d`, the squash merge of PR #92. PR #92 exact head `142cafc1623898c0f763f92ebadb4743869cca9f` passed Build and test #463, Native library scale smoke #133, M4 witness tool #27, M1 witness tool #27, M2 witness tool #22, M3 witness tool #17 and UI visual witness #23 before integration.
- Active development package: `fix/m1-device-switch-transport-continuity`, created from that exact `main`. Implementation head `021d703b4109346c9abceaa615200f4f4f072693` contains the code and deterministic core regressions preceding this status checkpoint; the PR and exact-final-head workflow run IDs are recorded after the checkpoint is published rather than guessed here.
- The M1 audit found a real engine-side device lifecycle defect: every stopped-audio `Engine::prepare()` reset loaded deck source-frame transport and audible cursors to zero. Because JUCE can prepare the application again after an output-device or sample-rate reconfiguration, that behavior could rewind an already loaded/playing deck even though the clip itself remained valid.
- The branch now preserves finite source-frame transport and audible cursors plus the current Reverse/Slip ownership across a stopped-audio reprepare. It still rebuilds device-rate-dependent resampler tables, fixed scratch, filter state, echo history and smoothers, and forces resumed playback through clean smoothing state instead of carrying stale DSP history across devices.
- Core regression coverage performs a 48 kHz -> 44.1 kHz reprepare during forward master/cue playback and a 48 kHz -> 96 kHz reprepare during split Reverse+Slip. The fixtures require preserved play intent, no rewind/large transport jump, finite master/cue output, continued hidden forward transport and continued audible reverse cursor ownership. These are deterministic engine tests only; they do not claim a real Windows audio-device switch, device-loss recovery or physical 4-output cue qualification.
- Exact-final-head Linux sanitizer/core checks, Windows Build/Test and the relevant package/witness-tool workflows must pass before integration. Any regression is repaired before merge.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 stay open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging and native four-deck async import/replacement isolation. PR #90 is integrated and makes the manual import/playback witness self-contained with a disposable synthetic WAV. The active device-continuity package fixes the deterministic engine-side rewind found during M1 review, but real Windows 11 clean-machine switching/loss and physical four-output master/cue isolation remain open.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle. Representative real-music BPM/key evidence, listening/latency qualification and physical controller workflows remain open. PR #91 is integrated and hardens only the evidence publication boundary; it does not satisfy those listening gates.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests. Physical/listening mic, Booth, limiter, recording and long-session gates remain open. PR #91 is integrated and hardens only the evidence publication boundary.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and native async session/adoption round-trip coverage. PR #92 is integrated and adds bounded off-thread file-presence reconciliation plus an explicit Library **Refresh file status** workflow so a moved source can be discovered before a failed load attempt. The connected Windows 11 witness is still required.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **M1:** real Windows 11 clean-machine launch/resize/import/playback, device-switch/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** reviewed connected Windows 11 library/session witness against the exact staged executable, including explicit file-status refresh after moving the synthetic fixture.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Run exact-final-head CI for `fix/m1-device-switch-transport-continuity` and repair every regression before integration. Keep M1 open even if the deterministic engine fixtures are green: a real Windows 11 device switch/loss and physical four-output cue test remain mandatory. Keep the Beta scope frozen; the largest remaining product gate is still the privacy-safe M4 connected-library/session witness, followed by the remaining physical M1–M3 hardware/listening evidence. Do not widen the target into M5+ merely to keep development busy.
