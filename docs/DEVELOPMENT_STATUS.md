# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `aa5124df8d8092899f6b91c91978188275060201`, following merged PR #56 and its durable status checkpoint.
- PR #55 (`M4: add bounded session persistence foundation`) is merged after exact-final-head workflow `35539900396` succeeded on Linux ASan/UBSan, Windows x64 build/full CTest/audio diagnostics/native no-audio GUI smoke/device probe, staging and downloaded-artifact smoke.
- PR #56 (`M4: connect native local library workflow`) is merged after exact-final-head workflow `35540530581` succeeded on the same Linux/Windows/package gates, including downloaded staged-artifact verification and no-audio smoke.
- Active development: PR #57 on `feat/m4-native-session-workflow`. Implementation/documentation checkpoint before this status commit: `c095841a6a7bb02287aace1b2539a0f78ca09609`; this status commit is part of the exact final head and must pass its own CI before merge.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). This M4 workflow does not close M4.
- GitHub Releases remains empty; no public BrokeDJ Release is qualified by this checkpoint.

## Integrated M4 foundations on main

- The local SQLite library foundation provides schema migrations, bounded search, tags, ordered playlists, play history, duplicate grouping, missing/moved-file rebinding, integrity checks and backup/restore without modifying original music.
- The session foundation provides bounded/versioned four-deck plus mixer snapshots, checksum/range validation, verified temporary publication, rollback handling and explicit verified backup recovery. Session I/O remains outside the realtime callback.
- The native PL/EN local-library workflow provides bounded type-ahead search, multi-file import, explicit target-deck loading and moved-file relocation. Search and writes use separate bounded single-worker paths with shutdown cancellation.

## Active PR #57: native session workflow

- The Library button now exposes Save Session, Load Session and explicit verified `.bak` recovery while keeping the local library workflow available.
- Session capture includes all four loaded paths/positions plus rate, trim, channel gain, LOW/MID/HIGH, echo, drive, cue, whole-track loop and master/crossfader/headphone state.
- Restore pauses all current transports first, loads each expected source through the normal asynchronous decoder, and only then applies the saved controls/position to that exact source. Saved `wasPlaying` never auto-starts audio.
- Missing/failed sources fail closed, restore waiting is bounded, and session/database/file I/O remains off the realtime callback. Session actions continue to work even if the local library database cannot open.
- Known pre-alpha limitation: an empty saved deck slot does not yet eject a previously loaded source in that slot; it remains paused and the restore dialog reports the limitation.

## Gates still open

- PR #57 must not merge until its exact-final-head Linux/Windows/package workflow is fully green, including native no-audio GUI construction/resize smoke and downloaded staged-artifact smoke.
- M4 still needs realtime-safe deck eject for exact empty-slot restore, moved-file resolution during session restore, playlist/tag editing UI, content-hash indexing, analysis/waveform-cache ownership, user-visible library backup/restore and real large-library qualification.
- M1 still requires real Windows 11 clean-machine/manual HiDPI/import/device-switching checks and physical master/cue/booth verification.
- Representative music-domain BPM/key/grid evidence, production key-lock listening/latency, controller profiles, long-session soak and physical recording/microphone evidence remain separate gates.

## Next largest step

First repair any exact-head PR #57 regression before integration. After the native session workflow is green, continue FINISH FIRST with realtime-safe deck eject plus moved-file session recovery, then playlist/tag management before widening optional DSP scope.
