# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline: `main` at `4cd5e5f069e1493fd8f3b2aa7023775e0b37ace5`, the squash merge of PR #91. PR #91 exact head `a16ce7e859975af9642f91c64dbf372b2c2b6bd3` passed Build and test #460, Native library scale smoke #130, M1 hardware witness #25, M2 key-lock witness #20, M3 mixer/recording witness #15 and Beta qualification #27 before integration.
- Active development package: `fix/m4-file-presence-refresh`, created from that baseline. Its branch-tip commit is the checkpoint containing this file; the PR and exact-head workflow run IDs are created only after the checkpoint is published and must be recorded in the PR/final run report rather than guessed here.
- The package closes a connected M4 usability gap found during witness review: a moved library source can now be discovered without first trying to load the broken path. `LibraryDatabase::refreshFilePresence()` snapshots ID/path/old missing state in bounded pages, probes the filesystem outside SQLite write transactions and uses conditional updates so a stale probe cannot overwrite a concurrently changed path/state.
- The Library menu exposes **Refresh file status / Odśwież status plików**. The workflow drains stale searches, runs 512-track pages on the existing maintenance worker, checks cancellation between pages, reports scanned/missing/changed/unresolved counts without logging private paths, and never performs filesystem I/O on the audio callback.
- A dedicated deterministic CTest moves a synthetic source away, proves paged missing discovery before any load attempt, checks `is:missing`, reconnects through Relocate, verifies stable ID/tag/playlist retention, restores an absent file and proves the stale missing flag clears. The M4 witness now explicitly requires this refresh before `is:missing` and treats unresolved probes as a blocker.
- Exact-head repository Build/Test, native library-scale/package checks and M4/Beta qualification checks are required before integration. No physical Windows 11 witness, hardware listening or release claim is created by these automated changes.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 stay open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging and native four-deck async import/replacement isolation. PR #90 is integrated and makes the manual import/playback witness self-contained with a disposable synthetic WAV while preserving the real clean-machine/device-switch/four-output hardware gates.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle. Representative real-music BPM/key evidence, listening/latency qualification and physical controller workflows remain open. PR #91 is integrated and hardens only the evidence publication boundary; it does not satisfy those listening gates.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests. Physical/listening mic, Booth, limiter, recording and long-session gates remain open. PR #91 is integrated and hardens only the evidence publication boundary.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and native async session/adoption round-trip coverage. The active package adds an explicit off-thread file-presence reconciliation path so missing review no longer depends on a failed load attempt. The connected Windows 11 witness is still required.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **M1:** real Windows 11 clean-machine launch/resize/import/playback, device-switch/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** reviewed connected Windows 11 library/session witness against the exact staged executable, including explicit file-status refresh after moving the synthetic fixture.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Run exact-head CI for `fix/m4-file-presence-refresh` and repair every regression before integration. Keep the Beta scope frozen. Once this package is green, the largest remaining product gate is the privacy-safe M4 connected-library/session witness on Windows 11 x64 using the integrated disposable fixture workspace and explicit file-status refresh, followed by the physical M1–M3 hardware/listening evidence. Do not widen the target into M5+ merely to keep development busy.
