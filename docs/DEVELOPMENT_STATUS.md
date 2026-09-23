# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline is `main` at `a69a0c104e2ff9f6d3cbd26d72127ecc066a81eb`, the squash merge of PR #94. Its exact PR head `e3c45a125f3530823198c4a5c021f5e6f65df939` passed Build and test #469, Native library scale smoke #139, UI visual witness #25, M1 hardware witness tool #29, M2 key-lock listening witness tool #24 and M3 mixer/recording witness tool #19 before integration.
- Active package is draft PR #95, branch `fix/m1-device-open-state`. Implementation head `ee203a73e5cf72e400d4fa5ffcb3d0c2dba36b69` tightens the merged M1 device-loss fail-safe so a JUCE device counts as available only while `AudioIODevice::isOpen()` remains true. Build and test #471 and Native library scale smoke #141 were queued for that implementation head when this checkpoint update began; this documentation commit creates a newer exact head that must receive its own required checks before merge.
- PR #94 already pauses every deck and clears continuous Sync ownership on a detected device loss while preserving transport; device recovery never auto-resumes playback. PR #95 fixes the remaining detection hole where `AudioDeviceManager` may retain a non-null device object after the underlying device has spontaneously closed.
- The availability check remains on the existing 20 Hz message-thread service. No device polling, I/O, allocation, logging or blocking synchronization was added to the audio callback.
- This is software fail-safe behavior only. It does not claim a physical Windows 11 unplug/replug test, device-loss timing, real four-output cue isolation, controller timing or reviewed listening.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 stay open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging, native four-deck async import/replacement isolation, deterministic device-reprepare transport continuity and merged conservative loss pause/no-auto-resume policy. PR #95 is a narrow follow-up to detect a selected JUCE device that has closed spontaneously even if its manager pointer remains non-null. The remaining M1 gate is real Windows 11 clean-machine switching/loss plus physical four-output master/cue isolation.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle. Representative real-music BPM/key evidence, key-lock listening/latency qualification and physical controller workflows remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests. Physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and native async session/adoption round-trip coverage. The connected Windows 11 witness is still required.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **M1:** exact-head PR #95 CI, then real Windows 11 clean-machine launch/resize/import/playback, device-switch/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** reviewed connected Windows 11 library/session witness against the exact staged executable, including explicit file-status refresh after moving the synthetic fixture.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Repair every exact-head regression on PR #95 before integration; do not merge while required checks are running or red. Keep the Beta scope frozen. After this narrow M1 detection hardening is green, the largest remaining product gate is the privacy-safe M4 connected-library/session witness on Windows 11 x64, followed by the physical M1 device/four-output checks and remaining M2–M3 listening/hardware evidence. If those manual gates cannot be executed in the current environment, preserve the verified software baseline instead of inventing substitute evidence.
