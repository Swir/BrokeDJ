# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline is `main` at `7e0cd8f082c334a6401c4c4edde2302fd2e47ca1`, the squash merge of PR #97. Its exact PR head `212e5a2aad18bfe10fdb6f8e0b75eb2aac9cb02e` passed Build and test #487, Native library scale smoke #157, UI visual witness #40, M1 hardware witness tool #40, M2 key-lock listening witness tool #35, M3 mixer/recording witness tool #30, M4 witness tool #28 and Beta qualification tool #28 before integration.
- Active package is draft PR #98, branch `fix/m4-fixture-stability-verification`. Product/test head before this checkpoint is `034b218609cf33079f12e60b4b03f8eef40e1e85`; its queued exact-head workflows are Build and test #489, Native library scale smoke #159, UI visual witness #41, M1 hardware witness tool #41, M2 key-lock listening witness tool #36, M3 mixer/recording witness tool #31, M4 witness tool #29 and Beta qualification tool #29. This documentation checkpoint creates a newer PR head that must receive its own required checks before integration.
- The first fixture presence pass may repair a stale persisted missing flag. The confirmation pass must be complete, report zero missing/unresolved rows and perform zero further persisted presence changes; aggregate track/missing/history/tag/playlist counts must also remain identical between snapshots.
- The two-pass policy is implemented in a JUCE-independent qualification helper and exercised directly plus through real SQLite/filesystem fixtures. Moving a generated duplicate between passes now fails qualification instead of allowing a transiently clean single snapshot to stand as M4 evidence.
- The verifier still makes no atomic-filesystem claim: a mutation after the confirmation pass remains outside its control and requires a later witness retry. No path, title, artist, tag name, playlist name or history timestamp is serialized.
- Filesystem probes and SQLite maintenance remain off the audio thread. This package does not claim that the connected Windows 11 M4 witness has actually been performed, and it does not replace physical M1–M3 hardware/listening evidence.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 stay open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging, native four-deck async import/replacement isolation, deterministic device-reprepare transport continuity, conservative loss pause/no-auto-resume behavior and retained-but-closed JUCE device detection via `AudioIODevice::isOpen()`. The remaining M1 gate is real Windows 11 clean-machine switching/loss plus physical four-output master/cue isolation.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle. Representative real-music BPM/key evidence, key-lock listening/latency qualification and physical controller workflows remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests. Physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and native async session/adoption round-trip coverage. PR #96 merged fail-closed stale-update accounting; PR #97 merged privacy-safe fixture-scoped verification and connected witness integration; the active package adds a two-pass stability confirmation before evidence acceptance. The actual reviewed Windows 11 witness is still required.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **M1:** real Windows 11 clean-machine launch/resize/import/playback, device-switch/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** exact-head CI for the active two-pass stability package, then perform and review the user-controlled Windows 11 library/session witness against that exact staged executable. The final witness still requires real launch/resize/import/search/tag/playlist/history/duplicate/missing/relocate/backup/restore/session interaction; the aggregate verifier only narrows persisted-state ambiguity during its bounded verification window.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Keep PR #98 draft, persist the newer exact branch head after this checkpoint, and repair every exact-head regression before integration. Keep the Beta scope frozen. Once this confirmation hardening is green, the remaining M4 blocker is the real Windows 11 connected witness using the strengthened per-run fixture and two-pass aggregate verifier. After reviewed M4 evidence, continue with physical M1 device/four-output checks and the remaining M2–M3 listening/hardware evidence. If those manual gates cannot be executed in the current environment, preserve the verified software baseline instead of inventing substitute evidence.
