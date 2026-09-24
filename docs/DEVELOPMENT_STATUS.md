# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Active development checkpoint — PR #106

- Active branch: `feat/beta-deck-platter-themes`; draft PR **#106** targets `main` (`6323b00673cc8764e4fc31dd64c9d44c4f9fab83`). The latest product-code checkpoint is `ed0d564c90d222004e98b9c264d0913455c72469`; this status update is a documentation-only successor, so the newest PR head remains the exact-head merge authority.
- The existing bounded Jog/Scratch transport is now presented as a compact circular platter beside each deck waveform instead of a generic horizontal strip. The platter adds a dedicated face, concentric deck rings, direction marker and centre hub while preserving the existing `JogScratchController` ownership, fail-closed Slip/Beat Loop rules and release-to-restore transport semantics.
- Three runtime accent themes remain in scope for this same Beta UX package: **Electric Blue**, **Ultraviolet** and **Ember**. Theme persistence stays on the message thread and does not touch audio/DSP state. Theme propagation now matches BrokeDJ-owned colours by RGB while preserving each control's original alpha, fixing partially blue controls such as the 90%-alpha slider track after switching theme.
- Semantic record/microphone/signal colours remain deliberately fixed; the theme changes presentation rather than transport/session/audio state. The user-facing selector remains on Deck A and the same menu is available by right-clicking a platter.
- Exact code-head CI started for `ed0d564c90d222004e98b9c264d0913455c72469`: Build and test **#539** (`35966095870`), UI visual witness **#57** (`35966095878`), Native library scale smoke **#209** (`35966095886`) and Beta witness runner **#15** (`35966095880`). At this checkpoint they are still running; no merge is claimed.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. This is a whole-roadmap milestone count, not first-Beta readiness, audio quality or live-readiness. M1–M4 remain open until their documented human acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Merged first-Beta workstation baseline

- `main` includes squash-merged PR #105 at `8e5ee002bf9a1c847e76d1fdc15bb6cb01af2bae` (`Make the first-Beta workstation panel usable at every supported size`). The exact code head qualified before merge was `b62b4935e488b6816423e21ce2ff33e8307d7749`.
- PR #105 passed every observed exact-head workflow before merge: Build and test #532 including Core/sanitizers, native Windows x64 build/full CTest and staged + extracted portable-package smoke; UI visual witness #52; Native library scale smoke #202; M1 hardware witness-tool #70; M2 key-lock witness-tool #65; M3 mixer/recording witness-tool #60; and Beta witness runner-tool #8.
- The smoke-qualified Windows x64 **Beta Preview development package** was produced from exact code head `b62b4935e488b6816423e21ce2ff33e8307d7749`. Its workflow artifact binds the staged tree, deterministic portable ZIP and source identity, and the portable ZIP was executed by CI after extraction into a relocated Windows path containing spaces/non-ASCII text. This is runnable development-preview evidence, not physical Windows 11 audio-hardware or listening qualification.
- The workstation keeps the real four-channel central mixer and channel faders visible throughout the supported workstation size range, adds independent per-deck MIX/GRID views, larger performance waveforms, sampled pre-fader peak strips, one-click Library plus separate Session controls, and explicit non-owning UI handles instead of child-order/caption discovery.
- Native no-audio smoke switches all four decks independently between GRID and MIX at every deterministic resize step and verifies visible-control containment/non-overlap plus preservation of transport/session settings. JUCE-independent `workspace_geometry` covers 53,186 content sizes.

## Verified implementation state

- **M1:** native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging, native four-deck async import/replacement isolation, deterministic device-reprepare transport continuity, conservative loss pause/no-auto-resume behavior, `AudioIODevice::isOpen()` loss detection and fast open-device A-to-B replacement fail-safe are implemented. The remaining gate is real Windows 11 clean-machine switching/loss plus physical four-output master/cue isolation.
- **M2:** reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle are implemented. Native `MainComponent`/DeckPanel integration covers PLAY/rate/LOOP/CUE 0, live seek and fail-closed clip replacement. PR #106 changes the presentation/interaction surface of the existing bounded Jog without claiming new scratch DSP. Representative real-music BPM/key review, key-lock listening/latency qualification and physical controller workflows remain open.
- **M3:** channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests are implemented. The merged workstation keeps the central mixer visible and adds sampled pre-fader peak strips without mislabelling them as true-peak/loudness meters. Physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- **M4:** SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery, native async session/adoption coverage and privacy-safe two-pass fixture-state verification are integrated. Library is a one-click action and Session owns save/load/backup commands. The actual human Windows 11 connected-library/session run remains required.
- **Packaging:** every portable ZIP member is bound byte-for-byte to the staged tree; deterministic ZIP metadata is checked; CI extracts into a relocated path containing spaces/non-ASCII text, runs the extracted `BrokeDJ.exe` through no-audio GUI/device-probe smoke, removes generated outputs and verifies the extracted payload again.
- **Qualification UX:** the guided runner coordinates the canonical M1–M4 human witnesses around one exact candidate executable and deliberately cannot fabricate hardware/listening evidence in CI.

## Unmet gates

- **PR #106:** the newest exact branch head must pass native Windows build/full CTest, no-audio resize/package smoke, UI visual witness, native library-scale smoke and the existing witness-tool regressions before merge. The Windows UI artifact must be reviewed for platter containment/readability; a compile-only pass is not enough for this presentation change.
- **M1:** real Windows 11 clean-machine launch/resize/import/playback, live device replacement/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification. Native synthetic integration tests are not substitutes for those human/device gates.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** perform and review the user-controlled Windows 11 connected library/session witness against an exact candidate executable. Exact-process ownership and schema-2 evidence remove identity ambiguity but do not fabricate launch/resize/import/search/tag/playlist/history/duplicate/missing/relocate/backup/restore/session evidence.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied. The current downloadable package is a development **Beta Preview** for hands-on testing, not a physically qualified public Beta/Release.

## Next largest step

Finish PR #106 rather than opening another UX branch: repair any exact-head Windows/UI regression, review the resulting real Windows UI witness, and merge only if the final head is green. Then exercise the smoke-qualified Windows x64 Beta Preview as the user-facing workstation candidate and use the integrated guided runner against that exact executable to complete genuine M1–M4 manual Windows 11/hardware/listening/library evidence. Fix any real panel, import, playback, device, recording or session regression before widening M5+ scope. Once those first-Beta gates pass, publish the appropriately labelled Beta with source/notices/checksum and known-issues review rather than treating automated CI alone as release qualification.
