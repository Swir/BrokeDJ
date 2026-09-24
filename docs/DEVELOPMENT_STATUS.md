# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline is `main` at `a99be7e248033156f494b58b189049dbeca0b397`, the squash merge of PR #99. Its push-main Build and test #494 and Native library scale smoke #164 completed successfully.
- Active package is draft PR #100, branch `test/m2-native-keylock-integration`. Exact head `67739059c417fcde094b01d3b8c624106c7818a2` exposed a second real Windows regression in Build and test #502 / run `35927364478`: `time_stretch_native_integration` failed at `native clip replacement preserves ordinary playback intent` after the earlier live-waveform-seek lifecycle defect had been repaired.
- Diagnosis confirmed the failing expectation contradicted the production Engine source-identity contract rather than exposing a playback implementation regression. `Engine::process()` deliberately stops `control.playing` when a newly submitted clip is adopted, resets transport/DSP epoch state and therefore requires explicit PLAY before the replacement source can render. Auto-continuing a different track would weaken this fail-closed boundary.
- Code checkpoints `6dc3216fe570eafcd34d179b6e22c25f23b87226` and `a3e1979fab41af5dd2774c4320847db022867e58` preserve that safer behavior and strengthen evidence instead of weakening the engine: core `EngineDeckSourceTests` now prove a live replacement stops before the optional renderer sees the new clip and emits no stale master/cue audio; the native `MainComponent` smoke now requires the same STOP boundary, silence before explicit PLAY, and key-lock rebinding to the generated 330 Hz replacement only after PLAY.
- No frequency threshold was relaxed to hide the failure. The obsolete `replacement fallback while still playing` expectation and metrics were replaced with a stricter stopped-output peak check plus the existing post-PLAY replacement pitch check. Live rate, LOOP and waveform-seek changes still prove audible production fallback because those are control discontinuities within the same source identity.
- Fresh workflows started for code head `a3e1979…`: Build and test #504 and Native library scale smoke #174 were pending, UI visual witness #48 was pending, and M1/M2/M3 witness-tool runs #52/#47/#42 were running when this checkpoint was prepared. This documentation commit creates a newer final branch head, so its own exact-head workflows must pass before integration.
- PR #100 continues to compile the real `MainComponent.cpp` with `BROKEDJ_TIMESTRETCH_PROTOTYPE=1`, import generated local WAV fixtures with no physical audio device, adopt them through the ordinary Engine callback boundary and drive the real DeckPanel PLAY/rate/LOOP/CUE 0 plus waveform-seek callbacks and async clip replacement.
- The package remains deterministic offline/native integration evidence only. It does not claim representative-music listening, physical device latency/deadline/underrun results, controller timing, or live-performance readiness.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 stay open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging, native four-deck async import/replacement isolation, deterministic device-reprepare transport continuity, conservative loss pause/no-auto-resume behavior, `AudioIODevice::isOpen()` loss detection and fast open-device A-to-B replacement fail-safe. The remaining M1 gate is real Windows 11 clean-machine switching/loss plus physical four-output master/cue isolation.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle. PR #100 now exercises actual `MainComponent`/DeckPanel playback, live rate/loop/seek discontinuities, CUE 0 and replacement-source boundaries against generated native fixtures; representative real-music BPM/key evidence, key-lock listening/latency qualification and physical controller workflows remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests. Physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and native async session/adoption round-trip coverage. PR #96 merged fail-closed stale-update accounting; PR #97 merged privacy-safe fixture-scoped verification and connected witness integration; PR #98 merged two-pass stability confirmation before evidence acceptance. The actual reviewed Windows 11 witness is still required.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **PR #100:** the newest exact branch head after this checkpoint must pass every required workflow. Build #502 is intentionally recorded as failed evidence; it is not ignored because unrelated witness/native checks were green. Any new compiler, CTest, package, UI or existing regression failure must be repaired before merge.
- **M1:** real Windows 11 clean-machine launch/resize/import/playback, live device replacement/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification. Native synthetic integration tests are not substitutes for those human/device gates.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** perform and review the user-controlled Windows 11 library/session witness against an exact staged executable. The final witness still requires real launch/resize/import/search/tag/playlist/history/duplicate/missing/relocate/backup/restore/session interaction; the aggregate verifier only narrows persisted-state ambiguity during its bounded verification window.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Let the newest exact-head PR #100 workflows exercise both replacement fail-closed regressions and the repaired live-seek lifecycle. Repair every regression they expose; do not merge while required checks are running or red. Once the accumulated PR package is exact-head green and coherent, integrate it according to the BrokeDJ development cadence. Keep the Beta scope frozen: the largest remaining product gate is the real Windows 11 M4 connected library/session witness, followed by physical M1 device/four-output checks and the remaining M2–M3 listening/hardware evidence. If those manual gates cannot be executed in the current environment, preserve the verified software baseline instead of inventing substitute evidence.
