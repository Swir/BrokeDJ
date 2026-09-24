# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline is `main` at `1db75e5961374522821bb9c8b41fb50e16e7c6a7`, the squash merge of PR #102 (`Harden portable Beta package integrity`). PR #102's final exact head passed its required observed controls before merge; post-merge Build/test #524 and Native library scale #194 also completed successfully on this exact `main` commit.
- Active package is draft PR #103, branch `tools/beta-witness-runner`. Its purpose is to remove avoidable operator fragmentation from the remaining first-Beta M1–M4 manual qualification without fabricating hardware, listening or library/session evidence.
- The runner binds one exact `BrokeDJ.exe` to one evidence directory, can create/reuse only the existing silent non-opening device probe, validates existing M1–M4 evidence before reuse, delegates M1 and M4 to their canonical interactive witnesses, and keeps M2/M3 playback, microphone and recording explicitly human-controlled before collecting the already-defined attestations.
- The runner exposes `-StatusOnly` for non-generating evidence status and `-ValidateExisting` for composed revalidation. Human evidence generation refuses CI before resolving or hashing `AppPath`; no CI path is allowed to mint accepted M1–M4 evidence.
- Initial PR #103 head `fc934efa5b9479b8b19ad50724a2def8b075ab33` exposed a PowerShell interpolation parser bug in the new Windows runner workflow. Commit `3d413a107325aa27473e63d517ad9d699a0ae26b` fixed parsing and passed both PowerShell 7 and Windows PowerShell 5.1 fixture self-tests. The next runner-contract failure was in the workflow's capture of the deliberately failing CI-refusal child process, not in the refusal itself; implementation head `1ec202f0b6bc52743439fe443848f1ad0954d54c` now captures stdout/stderr through `Start-Process` so the expected non-zero child cannot abort the parent shell before the boundary assertions execute.
- This documentation checkpoint follows `1ec202f0...`; its resulting exact PR head and workflows are the merge authority. Do not merge from an earlier green commit if this branch advances.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. This is a whole-roadmap milestone count, not first-Beta readiness, audio quality or live-readiness. M1–M4 remain open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- **M1:** native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging, native four-deck async import/replacement isolation, deterministic device-reprepare transport continuity, conservative loss pause/no-auto-resume behavior, `AudioIODevice::isOpen()` loss detection and fast open-device A-to-B replacement fail-safe are implemented. The remaining gate is real Windows 11 clean-machine switching/loss plus physical four-output master/cue isolation.
- **M2:** reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle are implemented. Native `MainComponent`/DeckPanel integration covers PLAY/rate/LOOP/CUE 0, live seek and fail-closed clip replacement. Representative real-music BPM/key review, key-lock listening/latency qualification and physical controller workflows remain open.
- **M3:** channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests are implemented. Physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- **M4:** SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery, native async session/adoption coverage and privacy-safe two-pass fixture-state verification are integrated. The connected witness is schema-2 and exact-process-bound; the actual human Windows 11 run remains required.
- **Packaging:** `main` now binds every portable ZIP member byte-for-byte to the staged tree, verifies deterministic ZIP metadata, extracts into a relocated Windows path containing spaces/non-ASCII text, runs the extracted `BrokeDJ.exe` through the no-audio smoke, then verifies the extracted payload again without verifier-created bytecode mutation.
- **Qualification UX (PR #103):** the new coordinator does not weaken milestone criteria. It sequences the canonical witnesses around one candidate, revalidates each output, and invokes the canonical Beta qualification summary only after M1–M4 all validate.

## Unmet gates

- **PR #103:** every workflow must pass on the newest exact branch head. The dedicated Windows runner contract must pass under PowerShell 7 and Windows PowerShell 5.1, prove unattended generation is refused before `AppPath` resolution, and coexist with the existing Build/test, native-scale and Beta-qualification workflows. Any regression must be repaired before merge.
- **M1:** real Windows 11 clean-machine launch/resize/import/playback, live device replacement/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification. Native synthetic integration tests are not substitutes for those human/device gates.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** perform and review the user-controlled Windows 11 connected library/session witness against an exact staged executable. Exact-process ownership and schema-2 evidence remove identity ambiguity but do not fabricate launch/resize/import/search/tag/playlist/history/duplicate/missing/relocate/backup/restore/session evidence.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied. The guided runner is an operator tool, not release evidence by itself.

## Next largest step

Repair any exact-head regression in PR #103 and merge only after all required controls are green. Once integrated, use the guided runner on a real Windows 11 x64 candidate to complete the genuine M1–M4 evidence set; M4 connected-library/session evidence remains the largest product gate, followed by physical M1 routing/device checks and M2–M3 listening/hardware evidence. If physical/manual gates cannot be executed in the current environment, continue closing only concrete first-Beta release blockers rather than inventing substitute evidence or widening into M5+.
