# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline is `main` at `25b29ff20a25e95f16104beaf024a0e354f37642`, the squash merge of PR #101. PR #101 exact head `df54cbeef21e0655df92754f408a7251875d7962` passed the required observed exact-head workflows before merge.
- Active package is draft PR #102, branch `fix/beta-portable-integrity-binding`. The archive-identity head `ea614999e8f4901758eed30a9556d52e284a83a5` passed the observed Build/test #518, native library scale #188 and M1/M2/M3 witness-tool runs #62/#57/#52 before this checkpoint was extended.
- Implementation checkpoint `6e5aaca1aa5f8957519900fe2c17f25b548c406e` now runs the **actual extracted portable ZIP** from a relocated Windows path containing spaces and a non-ASCII character. The extracted tree is inner-manifest verified, its exact `BrokeDJ.exe` runs the no-audio GUI lifecycle/resize smoke plus silent device-probe CI contract, generated smoke files are removed and the extracted payload is verified again.
- This status-only checkpoint follows that implementation commit; the newest PR head and its exact-head GitHub Actions are the merge authority. Do not merge PR #102 until the final head is green.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 remain open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- **M1:** native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging, native four-deck async import/replacement isolation, deterministic device-reprepare transport continuity, conservative loss pause/no-auto-resume behavior, `AudioIODevice::isOpen()` loss detection and fast open-device A-to-B replacement fail-safe are implemented. The remaining gate is real Windows 11 clean-machine switching/loss plus physical four-output master/cue isolation.
- **M2:** reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle are implemented. Native `MainComponent`/DeckPanel integration covers PLAY/rate/LOOP/CUE 0, live seek and fail-closed clip replacement. Representative real-music BPM/key review, key-lock listening/latency qualification and physical controller workflows remain open.
- **M3:** channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests are implemented. Physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- **M4:** SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery, native async session/adoption coverage and privacy-safe two-pass fixture-state verification are integrated. The connected witness is schema-2 and exact-process-bound; the actual human Windows 11 run remains required.
- **Packaging:** the staged package is manifest/checksum bound to source identity. PR #102 binds every portable ZIP member byte-for-byte to that staged tree, verifies deterministic ZIP metadata and now exercises the extracted portable executable from a relocated Unicode/space-containing path instead of qualifying only the loose staging tree.

## Unmet gates

- **PR #102:** every required workflow must pass on the newest exact branch head. Any Linux package-contract, Windows ZIP extraction/runtime smoke, staged-package smoke or existing regression failure must be repaired before merge.
- **M1:** real Windows 11 clean-machine launch/resize/import/playback, live device replacement/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification. Native synthetic integration tests are not substitutes for those human/device gates.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** perform and review the user-controlled Windows 11 library/session witness against an exact staged executable. Exact-process ownership and schema-2 evidence remove identity ambiguity but do not fabricate launch/resize/import/search/tag/playlist/history/duplicate/missing/relocate/backup/restore/session evidence.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Let PR #102's final exact-head workflows exercise both the staged tree and the **relocated extracted portable ZIP**. Repair every regression before integration. If green, keep the Beta scope frozen: the largest remaining product gate is the real Windows 11 M4 connected library/session run, followed by physical M1 device/four-output checks and the remaining M2–M3 listening/hardware evidence. If those manual gates cannot be executed in the current environment, continue hardening only concrete first-Beta release blockers instead of inventing substitute evidence.
