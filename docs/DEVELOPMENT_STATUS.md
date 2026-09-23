# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline is `main` at `a99be7e248033156f494b58b189049dbeca0b397`, the squash merge of PR #99. Its push-main Build and test #494 and Native library scale smoke #164 both completed successfully.
- Active package is draft PR #100, branch `test/m2-native-keylock-integration`. Prior exact head `a6979bbc4fadf572307aa8e5ca7eb0eef6c349b8` passed Build and test #498, Native library scale smoke #168, M1 hardware witness #46, M2 key-lock listening witness #41 and M3 mixer recording witness #36. Code checkpoint `7920acec6143196508e7f03d9a9336c52ad13ede` expands the same native integration test; this documentation checkpoint creates a newer final branch head, so fresh exact-head CI is required before integration.
- PR #100 compiles the real `MainComponent.cpp` with `BROKEDJ_TIMESTRETCH_PROTOTYPE=1`, imports generated local WAV fixtures with no physical audio device, adopts them through the ordinary Engine callback boundary and drives the real DeckPanel PLAY/rate/LOOP/CUE 0 plus waveform-seek callbacks.
- The fixture proves key-locked 1.20x playback stays near the 440 Hz source, while live rate and LOOP changes fail closed to the ordinary pitch-changing production converter until a paused restage returns near source pitch.
- Explicit CUE 0 stops and seeks before restage. The new live waveform-seek path keeps playback running, moves production transport near the requested midpoint, requires ordinary fallback while live and restores key lock only after a deliberate pause/restart.
- The same native fixture now replaces a playing 440 Hz clip with a separately decoded 330 Hz clip. It requires the old research snapshot to be discarded, verifies the new clip is adopted through ordinary production fallback at the current 1.10x rate, then proves paused restage binds key lock to the replacement source rather than leaking stale audio.
- `time_stretch_native_integration` prints locked/fallback/restaged frequency and RMS metrics for rate, LOOP, CUE 0, live waveform seek and live clip replacement into the existing `time_stretch` diagnostic replay. The integration target exists only when both the native app and opt-in time-stretch prototype are built; ordinary builds remain independent of Signalsmith.
- This package is deterministic offline/native integration evidence only. It does not claim representative-music listening, physical device latency/deadline/underrun results, controller timing, or live-performance readiness.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 stay open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging, native four-deck async import/replacement isolation, deterministic device-reprepare transport continuity, conservative loss pause/no-auto-resume behavior, `AudioIODevice::isOpen()` loss detection and fast open-device A-to-B replacement fail-safe. The remaining M1 gate is real Windows 11 clean-machine switching/loss plus physical four-output master/cue isolation.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle. PR #100 closes more of the native-integration evidence gap between the tested owner/lifecycle classes and actual `MainComponent`/DeckPanel transport, seek and replacement callbacks. Representative real-music BPM/key evidence, key-lock listening/latency qualification and physical controller workflows remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests. Physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and native async session/adoption round-trip coverage. PR #96 merged fail-closed stale-update accounting; PR #97 merged privacy-safe fixture-scoped verification and connected witness integration; PR #98 merged two-pass stability confirmation before evidence acceptance. The actual reviewed Windows 11 witness is still required.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **PR #100:** every required workflow must pass for the newest exact branch head after the live waveform-seek and replacement expansion. Any compiler, native-frequency, packaging or existing regression failure must be repaired before merge.
- **M1:** real Windows 11 clean-machine launch/resize/import/playback, live device replacement/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification. Native synthetic integration tests are not substitutes for those human/device gates.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** perform and review the user-controlled Windows 11 library/session witness against an exact staged executable. The final witness still requires real launch/resize/import/search/tag/playlist/history/duplicate/missing/relocate/backup/restore/session interaction; the aggregate verifier only narrows persisted-state ambiguity during its bounded verification window.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Repair every exact-head regression on the newest PR #100 head before integration and do not merge while required checks are running or red. If the expanded native key-lock integration evidence is green, keep the Beta scope frozen: the largest remaining product gate is still the real Windows 11 M4 connected library/session witness, followed by physical M1 device/four-output checks and the remaining M2–M3 listening/hardware evidence. If those manual gates cannot be executed in the current environment, preserve the verified software baseline instead of inventing substitute evidence.