# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline is `main` at `a366d73f6b490bdba23b24bbe90b55d9381a2b1d`, the squash merge of PR #100. PR #100 exact head `9d1d8168e0702ca9b04ece5e74ce4fbad94ebbf7` passed all eight observed required checks before merge, including Linux sanitizer/core, Windows development/staged-package, native library scale, UI visual and M1/M2/M3 witness-tool workflows.
- Push-main Build and test #506 has already completed its Core/sanitizer and Windows development-build jobs successfully; its staged-artifact smoke was still running at this checkpoint. Native library scale smoke #176 had completed the development-EXE 5k-track qualification and was still running the staged-EXE qualification. These post-merge runs are useful regression evidence but do not replace any hardware/listening witness.
- Active package is draft PR #101, branch `fix/m4-exact-app-witness-binding`. Its newest exact-head CI is the merge authority; do not merge from an earlier green commit after the branch advances.
- PR #101 removes an M4 evidence-identity ambiguity: the witness now rejects pre-existing BrokeDJ processes, launches the exact fingerprinted `AppPath` itself, tracks that PID and executable path around every manual answer, fails if a second BrokeDJ copy appears, and requires the tracked process to exit before the no-audio two-pass fixture-state verifier runs.
- The stronger ownership guarantee is now encoded in **M4 evidence schema 2**, with fixed `witness.interactionBinding = tracked-exact-app-process-v1` and `witness.fixtureStateVerification = two-pass-stable-aggregate-v1`. Legacy schema-1 M4 evidence is deliberately rejected rather than silently inheriting a guarantee it never recorded.
- M4 witness-tool CI now tests schema-2 positive evidence plus legacy-schema, wrong-binding, wrong-verifier, type, privacy, extra-field and fingerprint negatives. Beta qualification CI also verifies that an old schema-1 M4 witness cannot satisfy the composed M1–M4 Beta gate.
- Failure cleanup only closes/terminates the interactive process created by the witness. Existing BrokeDJ processes are rejected before launch rather than being killed. The witness still never starts fixture playback automatically and still requires human Windows 11 interaction.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 stay open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- **M1:** native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging, native four-deck async import/replacement isolation, deterministic device-reprepare transport continuity, conservative loss pause/no-auto-resume behavior, `AudioIODevice::isOpen()` loss detection and fast open-device A-to-B replacement fail-safe are implemented. The remaining gate is real Windows 11 clean-machine switching/loss plus physical four-output master/cue isolation.
- **M2:** reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle are implemented. PR #100 added native `MainComponent`/DeckPanel integration evidence across PLAY/rate/LOOP/CUE 0, live seek and fail-closed clip replacement. Representative real-music BPM/key review, key-lock listening/latency qualification and physical controller workflows remain open.
- **M3:** channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests are implemented. Physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- **M4:** SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery, native async session/adoption coverage and privacy-safe two-pass fixture-state verification are integrated. PR #101 strengthens the still-required connected witness by binding all manual work to one exact BrokeDJ process and versioning that guarantee into evidence.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity; with PR #101 it also requires the new exact-process-bound M4 schema. It cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **PR #101:** every workflow must pass on the newest exact branch head. Any PowerShell 5.1/7 parser/self-test, schema-2 evidence-contract, Beta-composition, build, package, native-scale, UI or existing regression failure must be repaired before merge.
- **M1:** real Windows 11 clean-machine launch/resize/import/playback, live device replacement/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification. Native synthetic integration tests are not substitutes for those human/device gates.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** perform and review the user-controlled Windows 11 library/session witness against an exact staged executable. Exact-process ownership and schema-2 evidence remove identity ambiguity but do not fabricate launch/resize/import/search/tag/playlist/history/duplicate/missing/relocate/backup/restore/session evidence.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Let PR #101's newest exact-head workflows exercise the exact-process witness on both PowerShell hosts, schema-2 negative cases, Beta composition and the normal build/package matrix. Repair every regression before integration. Keep the Beta scope frozen: once the witness tooling is green, the largest remaining gate is the real Windows 11 M4 connected library/session run itself, followed by physical M1 device/four-output checks and the remaining M2–M3 listening/hardware evidence. If those manual gates cannot be executed in the current environment, preserve the verified software baseline instead of inventing substitute evidence.
