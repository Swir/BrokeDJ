# BrokeDJ development status

This file is the durable engineering checkpoint for work that is not yet merged to `main`. It is not a release-readiness or live-performance claim.

## Current checkpoint

- Default branch: `main`.
- Verified `main` baseline: `eeb36f9367a75288101c04e61d9b9e54943b09ee` (merged PR #32).
- Baseline CI: run `35434471251` — successful Linux sanitizer/core and Windows x64 development-build gates.
- Active branch: `feat/hotcue-native-controls`.
- Pull request: #33, `M2: native persistent hotcue pads` — open, not merged.
- Implementation head before this documentation checkpoint: `a4863d664325ff8cbb834423a98837158b3e5d14`.
- Implementation CI: run `35434776740` — in progress at this checkpoint; merge is blocked until the exact final PR head is green.
- Roadmap source of truth remains `docs/progress.json`: 1/10 milestones complete (10.0%, PRE-ALPHA).
- No public BrokeDJ Release is qualified by this checkpoint.

## Implemented in PR #33

1. **Eight native Hot Cue pads per deck**
   - Compact `HC1`–`HC8` controls are part of the normal deck UI.
   - Clicking an empty pad stores the current source-time position; clicking a set pad jumps to it; Shift+click clears it.
   - New cues are quantized to the nearest beat only when a reviewed beat grid is available. Without a reviewed grid, a raw source-time cue is stored so ordinary playback is not blocked by analysis.
   - Triggering a Hot Cue preserves whole-track LOOP but exits an active beat-derived loop through the reviewed `PerformanceDeckOwner` contract.

2. **Source-bound persistence wired into the native lifecycle**
   - Track load resets stale in-memory cues before asynchronously restoring the exact source-bound `TrackHotCueStore` snapshot.
   - Store/load work is serialized on the existing analyzer worker; no persistence I/O was added to the audio callback.
   - Per-deck generation counters discard stale restore/write completions after replacement loads or newer cue edits.
   - Failed disk persistence does not discard the current session cue bank and is reported explicitly.
   - The optional key-lock research lifecycle is notified about accepted Hot Cue seeks so that path can safely restage or fall back.

3. **UI/runtime hardening**
   - Pads are disabled until a valid track duration is available and show set/unset state without changing the existing whole-track or beat-loop semantics.
   - Persistence restoration goes through transactional complete-bank owner validation; changed/out-of-range state fails closed.
   - The deck keeps a bounded waveform minimum while reserving one compact row for eight pads.

## Verification

Verified `main` baseline `eeb36f9367a75288101c04e61d9b9e54943b09ee` passed GitHub Actions run `35434471251`:

- Linux `ubuntu-24.04`: generated-progress check, sanitizer/core build and CTest — success.
- Windows `windows-2022`: native configure/build, full CTest, audio render/callback diagnostics, no-audio native GUI lifecycle smoke, development staging and artifact upload — success.

PR #33 implementation head `a4863d664325ff8cbb834423a98837158b3e5d14` is under exact-head run `35434776740`. At this checkpoint that run is still in progress, so no merge or readiness claim is made. This documentation commit creates a newer PR head and therefore also requires its own exact-head CI before merge.

The Windows CI smoke does not replace physical audio-interface, multi-output cue, controller, clean-machine listening or soak qualification.

## Gates still open

- Exact-final-head CI for PR #33 before merge.
- M1 physical Windows 11 audio-device qualification, including verified four-output cue routing where the interface supports it.
- Representative legal local-music corpus validation for BPM/key/grid behavior.
- Real listening/soak testing, controller testing and production release qualification.
- Beat Jump controls, bounded master/follower Sync UI/actuation policy, session-level migration and controller mappings.

## Next largest step

First finish exact-head CI and review for PR #33 and merge only if both Linux sanitizer/core and Windows x64 development gates are green. Then expose compact Beat Jump controls and a bounded one-shot master/follower Sync action that operates only on reviewed grids, preserves fail-closed behavior, and remains separate from any future continuous phase-lock implementation.
