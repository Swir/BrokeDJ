# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline is `main` at `3eb17ed8365afb392c710204bbf1fd116f85ef6f`, the squash merge of PR #96. Its exact PR head `58d0985a06c4a3fc6f01a3928fd4d0431ba1efc1` passed Build and test #482, Native library scale smoke #152, UI visual witness #36, M1 hardware witness tool #38, M2 key-lock listening witness tool #33 and M3 mixer/recording witness tool #28 before integration.
- Active package remains draft PR #97, branch `feat/m4-connected-fixture-verification`. Exact head `c1d20892b8d02f60a4d5681f087da4551c70fec7` passed Build and test #486, Native library scale smoke #156, UI visual witness #39, M1 hardware witness tool #39, M2 key-lock listening witness tool #34 and M3 mixer/recording witness tool #29. This checkpoint extends that same coherent M4 package and therefore requires a new exact-head gate before integration.
- PR #97 already provides bounded content-hash-scoped file-presence reconciliation plus a privacy-safe aggregate workflow summary for one exact SHA-256. The aggregate exposes only track/missing/history/tag-association/playlist-membership counts and cannot update unrelated user tracks.
- The human M4 witness is now wired to those primitives through a new no-audio `BrokeDJ.exe --m4-fixture-state` mode. After the operator completes the eight connected UI/recovery checks and closes BrokeDJ, the exact fingerprinted executable must prove at least two connected duplicate rows, at least one History/tag/playlist association and zero missing/unresolved fixture rows before evidence is accepted.
- Each witness run now generates a distinct synthetic WAV content identity while its duplicate remains byte-identical. This prevents stale rows from older witness runs from sharing the active fixture hash. `-FixtureSelfTest` checks cross-run hash uniqueness, exact duplicate identity, cleanup and positive/negative aggregate-report validation on both supported PowerShell hosts in CI.
- The temporary aggregate verifier serializes no source path, title, artist, tag name, playlist name or history timestamp, and its temporary report is deleted after validation. Accepted evidence keeps the existing closed schema/executable fingerprint contract, so Beta qualification compatibility is preserved.
- Filesystem probes and SQLite maintenance remain off the audio thread. This package still does not claim that the connected Windows 11 M4 witness has actually been performed, and it does not replace physical M1–M3 hardware/listening evidence.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 stay open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging, native four-deck async import/replacement isolation, deterministic device-reprepare transport continuity, conservative loss pause/no-auto-resume behavior and retained-but-closed JUCE device detection via `AudioIODevice::isOpen()`. The remaining M1 gate is real Windows 11 clean-machine switching/loss plus physical four-output master/cue isolation.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle. Representative real-music BPM/key evidence, key-lock listening/latency qualification and physical controller workflows remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests. Physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and native async session/adoption round-trip coverage. PR #96 merged fail-closed stale-update accounting; PR #97 adds privacy-safe fixture-scoped verification and the connected witness integration described above. The actual reviewed Windows 11 witness is still required.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **M1:** real Windows 11 clean-machine launch/resize/import/playback, device-switch/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** new exact-head PR #97 CI after the connected-verifier checkpoint, then perform and review the user-controlled Windows 11 library/session witness against that exact staged executable. The final witness still requires real launch/resize/import/search/tag/playlist/history/duplicate/missing/relocate/backup/restore/session interaction; the aggregate verifier only closes the persisted-state ambiguity.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Repair every exact-head regression from this PR #97 checkpoint before integration; do not merge while required checks are running or red. Keep the Beta scope frozen. Once green, the remaining M4 blocker is the real Windows 11 connected witness using the strengthened per-run fixture and aggregate verifier. After M4 evidence is reviewed, continue with the physical M1 device/four-output checks and remaining M2–M3 listening/hardware evidence. If those manual gates cannot be executed in the current environment, preserve the verified software baseline instead of inventing substitute evidence.
