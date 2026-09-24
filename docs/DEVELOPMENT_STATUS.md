# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline is `main` at `25b29ff20a25e95f16104beaf024a0e354f37642`, the squash merge of PR #101. PR #101 exact head `df54cbeef21e0655df92754f408a7251875d7962` passed all eight observed exact-head workflows before merge, including Build/test, native library scale, M1/M2/M3/M4 witness tooling and Beta qualification on both supported PowerShell hosts.
- PR #101 is now integrated: the M4 connected witness owns one exact fingerprinted BrokeDJ process, records schema-2 process/verifier binding and rejects legacy schema-1 evidence in both the M4 validator and composed Beta qualification gate. This strengthens evidence identity; it does not fabricate the still-required human Windows 11 witness.
- Active package is draft PR #102, branch `fix/beta-portable-integrity-binding`. Current head `0896d31f2e9c0e407cdc20fc0ece81071f8419e4` hardens the portable Beta Preview contract so each archived member must match the staged package by size and SHA-256 and must retain the deterministic ZIP timestamp/compression/file metadata written by the packager.
- PR #102 adds negative self-tests for two cases the previous verifier did not explicitly bind to the staged tree: a self-consistent re-manifested archive containing different payload bytes, and metadata-only ZIP drift with a recomputed outer sidecar. Local Python 3.13 syntax compilation and `package_contract.py self-test` pass for this head; GitHub exact-head workflows are the merge authority and are pending at this checkpoint.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 remain open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- **M1:** native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging, native four-deck async import/replacement isolation, deterministic device-reprepare transport continuity, conservative loss pause/no-auto-resume behavior, `AudioIODevice::isOpen()` loss detection and fast open-device A-to-B replacement fail-safe are implemented. The remaining gate is real Windows 11 clean-machine switching/loss plus physical four-output master/cue isolation.
- **M2:** reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle are implemented. Native `MainComponent`/DeckPanel integration covers PLAY/rate/LOOP/CUE 0, live seek and fail-closed clip replacement. Representative real-music BPM/key review, key-lock listening/latency qualification and physical controller workflows remain open.
- **M3:** channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests are implemented. Physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- **M4:** SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery, native async session/adoption coverage and privacy-safe two-pass fixture-state verification are integrated. The connected witness is now schema-2 and exact-process-bound; the actual human Windows 11 run remains required.
- **Packaging:** the staged package is manifest/checksum bound to source identity and generates a deterministic portable ZIP. PR #102 further binds the portable ZIP byte-for-byte to that exact staged tree and makes deterministic ZIP metadata part of verification rather than a creation-only convention.

## Unmet gates

- **PR #102:** every workflow must pass on the newest exact branch head. Any Linux package-contract self-test, Windows package creation/reverification or existing regression failure must be repaired before merge.
- **M1:** real Windows 11 clean-machine launch/resize/import/playback, live device replacement/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification. Native synthetic integration tests are not substitutes for those human/device gates.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** perform and review the user-controlled Windows 11 library/session witness against an exact staged executable. Exact-process ownership and schema-2 evidence remove identity ambiguity but do not fabricate launch/resize/import/search/tag/playlist/history/duplicate/missing/relocate/backup/restore/session evidence.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Let PR #102's newest exact-head workflows exercise the stronger archive/staged-tree identity contract on Linux and Windows packaging. Repair every regression before integration. If it is green, keep the Beta scope frozen: the largest remaining product gate is the real Windows 11 M4 connected library/session run, followed by physical M1 device/four-output checks and the remaining M2–M3 listening/hardware evidence. If those manual gates cannot be executed in the current environment, continue hardening only concrete first-Beta release blockers instead of inventing substitute evidence.
