# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline: `main` at `dc1b85b04b84d83aad2e9b4ae841503f50f74cdb`, the squash merge of PR #90. PR #90 exact head `0713e0aa5064b3f3176d0bf4a560401c7f03dcf7` passed Build and test #457, Native library scale smoke #127, M1 hardware witness tool #24 and Beta qualification tool #25 before integration.
- Active development package: `fix/m2-m3-witness-atomicity`, draft PR #91.
- Implementation checkpoint `5b688687b085236a58d5b46d416efe7c34986720` hardens both remaining human-listening evidence recorders without changing their acceptance scope: M2 key-lock and M3 mixer/recording generation now reject common truthy CI values before resolving `AppPath`, validate a sibling temporary candidate before publication, preserve previously accepted evidence when a retry is incomplete and remove temporary candidates on failure.
- The M2/M3 witness workflows now prove guard ordering with deliberately nonexistent application paths and exercise atomic-preservation semantics by attempting an incomplete retry against a previously accepted synthetic fixture. These workflow tests validate tooling only; they do not create human listening or hardware evidence.
- This status checkpoint advances the branch head after the implementation commit. Exact-final-head M2 witness-tool, M3 witness-tool, repository Build/Test and relevant Beta/package checks must pass before any integration.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 stay open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging and native four-deck async import/replacement isolation. PR #90 is integrated and makes the manual import/playback witness self-contained with a disposable synthetic WAV while preserving the real clean-machine/device-switch/four-output hardware gates.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle. Representative real-music BPM/key evidence, listening/latency qualification and physical controller workflows remain open. PR #91 hardens only the evidence publication boundary; it does not satisfy those listening gates.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests. Physical/listening mic, Booth, limiter, recording and long-session gates remain open. PR #91 hardens only the evidence publication boundary.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and native async session/adoption round-trip coverage. PR #89 is integrated and supplies disposable synthetic fixtures for the remaining connected Windows 11 witness.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **M1:** real Windows 11 clean-machine launch/resize/import/playback, device-switch/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** reviewed connected Windows 11 library/session witness against the exact staged executable.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Keep PR #91 on its development branch until its exact final head passes both witness-tool workflows plus repository Build/Test and relevant Beta/package checks; repair any regression before integration. Keep the Beta scope frozen. The largest remaining product gate is still the privacy-safe M4 connected-library/session witness on Windows 11 x64 using the integrated disposable fixture workspace, followed by the physical M1–M3 hardware/listening evidence. Do not widen the target into M5+ merely to keep development busy.
