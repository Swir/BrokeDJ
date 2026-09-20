# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch code baseline: `main` at merge commit `45542fa1a6fac36e21c2a56d53a44a7c4b6a2c10`; this status-only checkpoint follows that verified code snapshot.
- PR #55 (`M4: add bounded session persistence foundation`) is merged after exact-final-head workflow `35539900396` succeeded on Linux ASan/UBSan, Windows x64 build/full CTest/audio diagnostics/native no-audio GUI smoke/device probe, staging and downloaded-artifact smoke.
- PR #56 (`M4: connect native local library workflow`) is merged after exact-final-head workflow `35540530581` succeeded on the same Linux/Windows/package gates, including downloaded staged-artifact verification and no-audio smoke.
- No feature PR is required to represent these integrated M4 slices at this checkpoint.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). These M4 foundations do not close M4.
- GitHub Releases remains empty; no public BrokeDJ Release is qualified by this checkpoint.

## Integrated M4 foundations

- The local SQLite library foundation provides schema migrations, bounded search, tags, ordered playlists, play history, duplicate grouping, missing/moved-file rebinding, integrity checks and backup/restore without modifying original music.
- The session foundation provides bounded/versioned four-deck plus mixer snapshots, checksum/range validation, verified temporary publication, rollback handling and explicit verified backup recovery. Session I/O remains outside the realtime callback.
- The native PL/EN local-library workflow provides bounded type-ahead search, multi-file import, explicit target-deck loading and moved-file relocation. Search and writes use separate bounded single-worker paths with shutdown cancellation.
- Missing files fail closed and can be reconnected without changing stable track IDs. Direct deck loading remains available if the local library database cannot be opened.
- Library/session persistence remains local-only; no cloud account, upload service or network dependency was added.

## Gates still open

- M4 still needs playlist/tag editing UI, content-hash indexing, analysis/waveform-cache ownership, app-side Save Session / Load Session commands, missing/moved-track session restore behavior, user-visible backup/restore and real large-library qualification.
- M1 still requires real Windows 11 clean-machine/manual HiDPI/import/device-switching checks and physical master/cue/booth verification.
- Representative music-domain BPM/key/grid evidence, production key-lock listening/latency, controller profiles, long-session soak and physical recording/microphone evidence remain separate gates.

## Next largest step

Continue FINISH FIRST by wiring explicit Save Session / Load Session commands and restore-safe deck loading into the native app, then complete playlist/tag management before widening optional DSP scope.
