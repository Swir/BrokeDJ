# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline is `main` at `a99be7e248033156f494b58b189049dbeca0b397`, the squash merge of PR #99. Its push-main Build and test #494 and Native library scale smoke #164 both completed successfully.
- Active package is draft PR #100, branch `test/m2-native-keylock-integration`. Code/build head before this durable-status commit is `9b8db342785168307c18dea3c1118f6140e0f7c8`; the status commit creates a newer final branch head that must receive its own exact-head CI before integration.
- PR #100 adds a native no-device integration fixture that compiles the real `MainComponent.cpp` with `BROKEDJ_TIMESTRETCH_PROTOTYPE=1`, imports an 8-second generated 440 Hz WAV, adopts it through the ordinary Engine callback boundary and drives the real DeckPanel PLAY/rate callbacks.
- The fixture measures three native states: key-locked 1.20x playback should remain near the 440 Hz source; a live rate edit to 1.10x must fail closed to the ordinary pitch-changing production converter near 484 Hz while playback continues; pausing and pressing PLAY again must restage the optional renderer and return near 440 Hz.
- The new CTest is named `time_stretch_native_integration`, so the existing verbose `time_stretch` diagnostic replay automatically retains its locked/fallback/restaged frequency and RMS metrics in the Windows development artifact without adding another workflow path.
- The integration target exists only when both the native app and the opt-in time-stretch prototype are built. Ordinary builds remain independent of Signalsmith and no user-facing production key-lock claim is added.
- This package is deterministic offline/native integration evidence only. It does not claim representative-music listening, physical device latency/deadline/underrun results, controller timing, or live-performance readiness.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 stay open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging, native four-deck async import/replacement isolation, deterministic device-reprepare transport continuity, conservative loss pause/no-auto-resume behavior, `AudioIODevice::isOpen()` loss detection and fast open-device A-to-B replacement fail-safe. The remaining M1 gate is real Windows 11 clean-machine switching/loss plus physical four-output master/cue isolation.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle. PR #100 is intended to close an internal native-integration evidence gap between the already-tested owner/lifecycle classes and the actual `MainComponent`/DeckPanel callbacks. Representative real-music BPM/key evidence, key-lock listening/latency qualification and physical controller workflows remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests. Physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and native async session/adoption round-trip coverage. PR #96 merged fail-closed stale-update accounting; PR #97 merged privacy-safe fixture-scoped verification and connected witness integration; PR #98 merged two-pass stability confirmation before evidence acceptance. The actual reviewed Windows 11 witness is still required.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **PR #100:** every required workflow must pass for the exact final branch head. Any compiler, native-frequency, packaging or existing regression failure must be repaired before merge.
- **M1:** real Windows 11 clean-machine launch/resize/import/playback, live device replacement/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification. The new native integration test is not a substitute for those human/device gates.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** perform and review the user-controlled Windows 11 library/session witness against an exact staged executable. The final witness still requires real launch/resize/import/search/tag/playlist/history/duplicate/missing/relocate/backup/restore/session interaction; the aggregate verifier only narrows persisted-state ambiguity during its bounded verification window.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Repair every exact-head regression on PR #100 before integration and do not merge while required checks are running or red. If the native key-lock integration evidence is green, keep the Beta scope frozen: the largest remaining product gate is still the real Windows 11 M4 connected library/session witness, followed by physical M1 device/four-output checks and the remaining M2–M3 listening/hardware evidence. If those manual gates cannot be executed in the current environment, preserve the verified software baseline instead of inventing substitute evidence.
