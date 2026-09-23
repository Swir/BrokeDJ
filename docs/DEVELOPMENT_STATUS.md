# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline is `main` at `a9c0a31728c96c4c3adeaaa5723dfef5c1aa631a`, the squash merge of PR #98. Its push-main Build and test #491 and Native library scale smoke #161 both passed after integration. PR #98 itself had passed all required exact-head workflows before merge.
- Active package is draft PR #99, branch `fix/m1-device-replacement-fail-safe`. Product head `9e649e3c3c4ce3d84b06021101382bb60a9bd8af` adds identity-aware device replacement handling; this checkpoint commit creates a newer final branch head that must receive its own exact-head CI before integration.
- M1 recovery no longer relies only on observing an unavailable audio device. The JUCE-independent policy now accepts an optional process-local identity token, so an open device A replaced by a different open device B between 20 Hz message-thread polls triggers one conservative playback pause and requires explicit PLAY to resume.
- The identity token is the live `AudioIODevice*` address only. It is neither persisted nor logged and is ignored when unknown, preventing the compatibility path from inventing replacement events. Compile-time contracts cover same-device stability, one-shot A-to-B replacement, unknown identities, loss/recovery and repeated loss cycles.
- Native device observation and recovery decisions remain on the existing message-thread timer. No device polling, allocation, disk/network I/O, locking or recovery work was added to the audio callback.
- This package is software fail-safe work only. It does not claim a physical Windows 11 device switch/loss test, four-output cue isolation, controller timing or reviewed listening.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 stay open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging, native four-deck async import/replacement isolation, deterministic device-reprepare transport continuity, conservative loss pause/no-auto-resume behavior and retained-but-closed JUCE device detection via `AudioIODevice::isOpen()`. PR #99 additionally covers an internally actionable fast live-device A-to-B replacement gap, pending exact-head CI and physical validation. The remaining M1 gate is real Windows 11 clean-machine switching/loss plus physical four-output master/cue isolation.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle. Representative real-music BPM/key evidence, key-lock listening/latency qualification and physical controller workflows remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests. Physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and native async session/adoption round-trip coverage. PR #96 merged fail-closed stale-update accounting; PR #97 merged privacy-safe fixture-scoped verification and connected witness integration; PR #98 merged two-pass stability confirmation before evidence acceptance. The actual reviewed Windows 11 witness is still required.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **M1:** exact-head CI for PR #99, then real Windows 11 clean-machine launch/resize/import/playback, live device replacement/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** perform and review the user-controlled Windows 11 library/session witness against an exact staged executable. The final witness still requires real launch/resize/import/search/tag/playlist/history/duplicate/missing/relocate/backup/restore/session interaction; the aggregate verifier only narrows persisted-state ambiguity during its bounded verification window.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Repair every exact-head regression on PR #99 before integration; do not merge while required checks are running or red. Keep the Beta scope frozen. Once this M1 fail-safe package is green, the largest remaining product gate is still the real Windows 11 M4 connected library/session witness, followed by physical M1 device/four-output checks and the remaining M2–M3 listening/hardware evidence. If those manual gates cannot be executed in the current environment, preserve the verified software baseline instead of inventing substitute evidence.
