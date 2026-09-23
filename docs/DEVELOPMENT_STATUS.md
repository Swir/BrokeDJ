# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline is `main` at `a32f0a5fcbd121fd34f295a1b1cbe17ff9301261`. Its push-main Build and test #467 and Native library scale smoke #137 both passed after PR #93 integration.
- Active package is draft PR #94, branch `fix/m1-device-loss-fail-safe`. Implementation head `6b08994e5257a3a8857b12aba808049261df5825` started Build and test #468, Native library scale smoke #138 and UI visual witness #24; all were queued when this checkpoint was written. This status commit is documentation-only and therefore creates a newer PR head that must receive its own exact-head checks before merge.
- The package closes an internally actionable M1 safety gap left after PR #93: when the native app observes that an active audio device has disappeared, it now pauses every deck and clears continuous Sync ownership while preserving deck/session transport positions. A later device return does not auto-resume playback; the DJ must explicitly press PLAY.
- Device availability is observed from the existing message-thread timer. The new JUCE-independent `AudioDeviceRecoveryPolicy` has compile-time contracts for neutral startup, one-shot loss, repeated unavailable observations, recovery without auto-resume, initial-unavailable startup and repeated loss/recovery cycles. No device I/O, polling, allocation or recovery work was added to the audio callback.
- This is software fail-safe behavior only. It does not claim a physical Windows 11 unplug/replug test, device-loss timing, real four-output cue isolation, controller timing or reviewed listening.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 stay open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging, native four-deck async import/replacement isolation and deterministic device-reprepare transport continuity. PR #94 adds conservative device-loss pause/no-auto-resume behavior but still requires exact-head CI and physical validation. The remaining M1 gate is real Windows 11 clean-machine switching/loss plus physical four-output master/cue isolation.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle. Representative real-music BPM/key evidence, key-lock listening/latency qualification and physical controller workflows remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests. Physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and native async session/adoption round-trip coverage. The connected Windows 11 witness is still required.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **M1:** exact-head PR #94 CI, then real Windows 11 clean-machine launch/resize/import/playback, device-switch/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** reviewed connected Windows 11 library/session witness against the exact staged executable, including explicit file-status refresh after moving the synthetic fixture.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Repair every exact-head regression on PR #94 before integration; do not merge while required checks are running or red. Keep the Beta scope frozen. After this M1 fail-safe package is green, the largest remaining product gate is still the privacy-safe M4 connected-library/session witness on Windows 11 x64, followed by the physical M1 device/four-output checks and remaining M2–M3 listening/hardware evidence. If those manual gates cannot be executed in the current environment, preserve the verified software baseline instead of inventing substitute evidence.
