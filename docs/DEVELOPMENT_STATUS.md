# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline is `main` at `a99be7e248033156f494b58b189049dbeca0b397`, the squash merge of PR #99. Its push-main Build and test #494 and Native library scale smoke #164 both completed successfully.
- Active package is draft PR #100, branch `test/m2-native-keylock-integration`. Exact head `8cefa8f6a64b6c3626aa0c134ce4348942c73533` exposed a real Windows regression in Build and test #500 / run `35921227293`: `time_stretch_native_integration` failed the assertion `paused native restage restores key lock after live waveform seek`. The other reported native/core gates were not treated as a substitute for this red required check.
- Root cause was lifecycle ownership after a live seek. `noteSeekNormalized()` retained the original explicit seek cursor even though the deck was still playing through the production fallback; by the time the DJ paused, fallback transport had advanced, so the later key-lock restage could publish a source snapshot at stale seek time and immediately fail the renderer's cursor identity boundary.
- Code checkpoint `1f0d4d6a89919941298591ba269c01fba3b8d360` fixes that ownership rule without widening product scope: a seek issued while playback is active still disarms key lock and marks the deck dirty, but it deliberately does not preserve the original explicit cursor. The later paused restage uses the authoritative current transport position. A seek issued while already stopped still retains its explicit immutable-clip cursor, preserving deterministic CUE 0 / paused-seek staging before Engine consumes the seek mailbox.
- The existing native integration regression is intentionally kept as the end-to-end gate: it drives the real DeckPanel waveform callback while live, verifies production fallback, pauses, then requires key-locked source-pitch restoration on the next PLAY. No threshold was weakened and no failing assertion was removed.
- For code checkpoint `1f0d4d6…`, M1 hardware witness #49, M2 key-lock listening witness #44 and M3 mixer recording witness #39 completed successfully. Build and test #501, Native library scale smoke #171 and UI visual witness #45 were still running when this checkpoint text was prepared. This documentation commit creates a newer final branch head, so fresh exact-head checks are required before integration even if #501 finishes green.
- PR #100 otherwise continues to compile the real `MainComponent.cpp` with `BROKEDJ_TIMESTRETCH_PROTOTYPE=1`, import generated local WAV fixtures with no physical audio device, adopt them through the ordinary Engine callback boundary and drive the real DeckPanel PLAY/rate/LOOP/CUE 0 plus waveform-seek callbacks and async clip replacement.
- The package remains deterministic offline/native integration evidence only. It does not claim representative-music listening, physical device latency/deadline/underrun results, controller timing, or live-performance readiness.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 stay open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging, native four-deck async import/replacement isolation, deterministic device-reprepare transport continuity, conservative loss pause/no-auto-resume behavior, `AudioIODevice::isOpen()` loss detection and fast open-device A-to-B replacement fail-safe. The remaining M1 gate is real Windows 11 clean-machine switching/loss plus physical four-output master/cue isolation.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle. PR #100 closes more of the native-integration evidence gap between the tested owner/lifecycle classes and actual `MainComponent`/DeckPanel transport, seek and replacement callbacks. Representative real-music BPM/key evidence, key-lock listening/latency qualification and physical controller workflows remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests. Physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and native async session/adoption round-trip coverage. PR #96 merged fail-closed stale-update accounting; PR #97 merged privacy-safe fixture-scoped verification and connected witness integration; PR #98 merged two-pass stability confirmation before evidence acceptance. The actual reviewed Windows 11 witness is still required.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **PR #100:** fresh required workflows must pass for the newest exact branch head after the live-seek lifecycle repair and this durable checkpoint. Any compiler, native-frequency, package, UI or existing regression failure must be repaired before merge.
- **M1:** real Windows 11 clean-machine launch/resize/import/playback, live device replacement/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification. Native synthetic integration tests are not substitutes for those human/device gates.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** perform and review the user-controlled Windows 11 library/session witness against an exact staged executable. The final witness still requires real launch/resize/import/search/tag/playlist/history/duplicate/missing/relocate/backup/restore/session interaction; the aggregate verifier only narrows persisted-state ambiguity during its bounded verification window.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Let the newest exact-head PR #100 workflows exercise the repaired live-seek ownership rule. Repair every regression they expose; do not merge while required checks are running or red. Once the accumulated PR package is exact-head green and coherent, integrate it according to the BrokeDJ development cadence. Keep the Beta scope frozen: the largest remaining product gate is the real Windows 11 M4 connected library/session witness, followed by physical M1 device/four-output checks and the remaining M2–M3 listening/hardware evidence. If those manual gates cannot be executed in the current environment, preserve the verified software baseline instead of inventing substitute evidence.
