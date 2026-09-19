# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness or live-performance claim.

## Current checkpoint

- Default branch: `main`.
- Current integrated code baseline before this documentation-only checkpoint: `57c061c33592a93678f9b928e8f350afe5956564` (merged PR #33).
- PR #33 final head: `219e48ed8133c1d27d0d069ce91b2a6ad4727724`.
- Exact PR-head CI: run `35434862222` — successful Linux sanitizer/core and Windows x64 development-build gates, including audio diagnostics, native GUI smoke, staging and artifact upload.
- Post-merge `main` run `35435252504` was queued when this checkpoint was written; this documentation commit creates a newer `main` head and therefore requires its own push CI before it can be called the latest verified `main` snapshot.
- Roadmap source of truth remains `docs/progress.json`: 1/10 milestones complete (10.0%, PRE-ALPHA).
- No public BrokeDJ Release is qualified by this checkpoint.

## Integrated this run

1. **Source-bound persistent Hot Cue state**
   - Versioned eight-slot snapshots backed by the exact `PerformanceDeckOwner::HotCue` model.
   - State is bound to source size and modification time, uses a non-plaintext source key, omits raw source paths/filenames from payloads, limits payloads to 16 KiB and uses temporary-file replacement.
   - Complete cue banks are validated transactionally before restoration; invalid or changed-source state fails closed.

2. **Eight native Hot Cue pads per deck**
   - Compact `HC1`–`HC8` controls are part of the normal deck UI.
   - Clicking an empty pad stores the current source-time position; clicking a set pad jumps to it; Shift+click clears it.
   - New cues are quantized to the nearest beat only when a reviewed beat grid is available. Without a reviewed grid, a raw source-time cue is stored so ordinary playback is not blocked by analysis.
   - Triggering a Hot Cue preserves whole-track LOOP but exits an active beat-derived loop through the reviewed `PerformanceDeckOwner` contract.

3. **Native lifecycle and realtime hardening**
   - Track load resets stale in-memory cues before asynchronously restoring the exact source-bound snapshot.
   - Store/load work is serialized on the existing analyzer worker; no persistence I/O was added to the audio callback.
   - Per-deck generation counters discard stale restore/write completions after replacement loads or newer cue edits.
   - Failed disk persistence leaves the current session cue bank active and is reported explicitly.
   - The optional key-lock research lifecycle is notified about accepted Hot Cue seeks so that path can safely restage or fall back.

## Verification

Exact PR #33 head `219e48ed8133c1d27d0d069ce91b2a6ad4727724` passed GitHub Actions run `35434862222`:

- Linux `ubuntu-24.04`: generated-progress check, sanitizer/core build and full CTest — success.
- Windows `windows-2022`: native configure/build, full CTest, audio render/callback diagnostics, no-audio native GUI lifecycle smoke, development staging and artifact upload — success.

The Windows CI smoke does not replace physical audio-interface, multi-output cue, controller, clean-machine listening or soak qualification.

## Gates still open

- Latest `main` push CI for this documentation checkpoint.
- M1 physical Windows 11 audio-device qualification, including verified four-output cue routing where the interface supports it.
- Representative legal local-music corpus validation for BPM/key/grid behavior.
- Real listening/soak testing, controller testing and production release qualification.
- Beat Jump controls, bounded master/follower Sync UI/actuation policy, session-level migration and controller mappings.

## Next largest step

After the latest `main` push CI is green, continue M2 with compact Beat Jump controls and a bounded one-shot master/follower Sync action that operates only on reviewed grids, preserves fail-closed behavior, and remains separate from any future continuous phase-lock implementation.
