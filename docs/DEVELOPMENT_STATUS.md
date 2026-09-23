# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline: `main` at `7f6e18d37624ec0447918c8423c0e53125ac2ede`, the squash merge of PR #89. Its push-main Build and test #454 and Native library scale smoke #124 both passed.
- Active development package: `feat/m1-self-contained-witness`, draft PR #90.
- Implementation checkpoint `0047e7882c2c279ea358997b4476c665153ef65b` makes the M1 hardware witness self-contained and fail-closed: it generates a disposable 48 kHz stereo 16-bit PCM playback fixture, rejects CI evidence generation before resolving `AppPath`, refuses to publish when any human check fails, and validates a temporary candidate before replacing accepted evidence.
- The M1 witness workflow now exercises fixture generation/validation/cleanup on PowerShell 7 and built-in Windows PowerShell 5.1. Its CI-generation negative cases use a deliberately nonexistent `AppPath`, proving the unattended guard executes before application resolution rather than failing accidentally on a valid fixture executable.
- PR #90 initial exact implementation head queued M1 hardware witness tool #22 and Build and test #455. This status checkpoint advances the branch head and must itself receive the required final-head checks before integration; no hardware evidence is inferred from tool CI.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 stay open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging and native four-deck async import/replacement isolation. PR #90 removes the need for personal music during its import/playback witness and hardens evidence publication; real clean-machine playback, device switching and physical master 1/2 versus CUE 3/4 isolation remain manual.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle. Representative real-music BPM/key evidence, listening/latency qualification and physical controller workflows remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests. Physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and native async session/adoption round-trip coverage. PR #89 is integrated and supplies disposable synthetic fixtures for the remaining connected Windows 11 witness.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **M1:** real Windows 11 clean-machine launch/resize/import/playback, device-switch/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** reviewed connected Windows 11 library/session witness against the exact staged executable.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Keep PR #90 on its development branch until its exact final head passes the M1 witness-tool workflow plus repository Build/Test checks; repair any regression before integration. Keep the Beta scope frozen. The largest remaining product gate is still the privacy-safe M4 connected-library/session witness on Windows 11 x64 using the integrated disposable fixture workspace, followed by the physical M1–M3 hardware/listening evidence. Do not widen the target into M5+ merely to keep development busy.
