# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline is `main` at `bcf4615b2d3a3349652bac343c4c57be254451bb`, the squash merge of PR #103 (`Add guided first-Beta witness runner`). PR #102 portable-package integrity hardening and PR #103 guided witness tooling are therefore both integrated on `main`.
- Active package is draft PR #105, branch `feat/beta-workspace-panel`. This package responds directly to the user-facing Beta priority: complete the real DJ workstation panel and deliver a runnable Windows Beta Preview instead of adding another qualification wrapper.
- PR #105 keeps the four-channel central mixer and vertical faders visible at every supported size, adds independent MIX/GRID deck views, larger performance waveforms, sampled pre-fader peak strips, one-click Library and a separate Session menu, and replaces fragile child-order/caption discovery with explicit non-owning UI handles.
- The native no-audio smoke now switches all four decks independently between GRID and MIX at each resize step and checks visible-control containment, non-overlap and unchanged session/transport settings. JUCE-independent `workspace_geometry` covers 53,186 content sizes.
- The branch has been reconciled with current `main` without force-push or dropping PR #103 files. The exact merged branch head and its workflows are the merge authority; do not merge from the earlier pre-reconciliation head.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. This is a whole-roadmap milestone count, not first-Beta readiness, audio quality or live-readiness. M1–M4 remain open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- **M1:** native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging, native four-deck async import/replacement isolation, deterministic device-reprepare transport continuity, conservative loss pause/no-auto-resume behavior, `AudioIODevice::isOpen()` loss detection and fast open-device A-to-B replacement fail-safe are implemented. The remaining gate is real Windows 11 clean-machine switching/loss plus physical four-output master/cue isolation.
- **M2:** reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle are implemented. Native `MainComponent`/DeckPanel integration covers PLAY/rate/LOOP/CUE 0, live seek and fail-closed clip replacement. PR #105 makes the performance surface consistently accessible across supported window sizes. Representative real-music BPM/key review, key-lock listening/latency qualification and physical controller workflows remain open.
- **M3:** channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests are implemented. PR #105 keeps the real central mixer visible and adds sampled pre-fader peak strips without mislabelling them as true-peak/loudness meters. Physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- **M4:** SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery, native async session/adoption coverage and privacy-safe two-pass fixture-state verification are integrated. PR #105 separates one-click Library from Session save/load/backup actions. The actual human Windows 11 connected-library/session run remains required.
- **Packaging:** `main` binds every portable ZIP member byte-for-byte to the staged tree, verifies deterministic ZIP metadata, extracts into a relocated Windows path containing spaces/non-ASCII text, runs the extracted `BrokeDJ.exe` through the no-audio smoke, then verifies the extracted payload again without verifier-created bytecode mutation.
- **Qualification UX:** the integrated guided runner coordinates the canonical M1–M4 witnesses around one exact candidate executable and does not weaken hardware/listening criteria.

## Unmet gates

- **PR #105:** newest exact-head Linux/core/geometry, Windows x64 build and full CTest, native resize/mode smoke, UI visual capture and extracted portable-package smoke must all pass. Any regression must be repaired before merge.
- **M1:** real Windows 11 clean-machine launch/resize/import/playback, live device replacement/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification. Native synthetic integration tests are not substitutes for those human/device gates.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** perform and review the user-controlled Windows 11 connected library/session witness against an exact staged executable. Exact-process ownership and schema-2 evidence remove identity ambiguity but do not fabricate launch/resize/import/search/tag/playlist/history/duplicate/missing/relocate/backup/restore/session evidence.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied. A downloadable development Beta Preview may be supplied for hands-on testing once its exact package smoke passes; it must not be described as a physically qualified public Beta/Release.

## Next largest step

Run PR #105's exact reconciled head through the native Windows build, resize/MIX-GRID smoke, visual capture review and extracted portable-package smoke. Repair every regression before integration. When those automated/runtime gates are green, provide the resulting Windows x64 **Beta Preview development package** for hands-on feedback on the complete workstation panel. After that, use the integrated guided runner on the exact candidate to complete the genuine M1–M4 manual evidence set rather than widening M5+ scope.
