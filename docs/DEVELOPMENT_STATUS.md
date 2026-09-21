# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch code baseline: `main` at feature merge `6d0351e31ce89e95ae4e3c17c3b0c6488b823069` from PR #59 (`M4: recover moved session sources and expose library backups`).
- PR #55 (`M4: add bounded session persistence foundation`) merged after exact-final-head workflow `35539900396` succeeded on Linux ASan/UBSan, Windows x64 build/full CTest/audio diagnostics/native no-audio GUI smoke/device probe, staging and downloaded-artifact smoke.
- PR #56 (`M4: connect native local library workflow`) merged after exact-final-head workflow `35540530581` succeeded on the same Linux/Windows/package gates, including downloaded staged-artifact verification and no-audio smoke.
- PR #57 (`M4: add native Save Load Session workflow`) merged after exact-final-head workflow `35541843965` succeeded on Linux ASan/UBSan, Windows x64 build/full CTest/audio diagnostics/native no-audio GUI smoke/device probe, staging/upload and downloaded staged-artifact smoke.
- PR #59 final head `bff7579c5c410b5dcf5cce8bb2e26ca2eee535bf` passed exact-head workflow `35544313308` and was squash-merged as `6d0351e31ce89e95ae4e3c17c3b0c6488b823069`. The merged-main push workflow `35546605929` also completed successfully.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). These M4 foundations do not close M4.
- GitHub Releases remains empty; no public BrokeDJ Release is qualified by this checkpoint.

## Integrated M4 foundations on main

- The local SQLite library foundation provides schema migrations, bounded search, tags, ordered playlists, play history, duplicate grouping, missing/moved-file rebinding, integrity checks and backup/restore without modifying original music.
- The session foundation provides bounded/versioned four-deck plus mixer snapshots, checksum/range validation, verified temporary publication, rollback handling and explicit verified backup recovery. Session I/O remains outside the realtime callback.
- The native PL/EN local-library workflow provides bounded type-ahead search, multi-file import, explicit target-deck loading and moved-file relocation. Search and writes use separate bounded single-worker paths with shutdown cancellation.
- Native Save Session / Load Session / verified `.bak` recovery is wired into the application. Restore pauses all transports, loads sources through the normal async decoder and does not auto-resume saved `wasPlaying` state.
- Missing session sources can be reselected per deck through the normal asynchronous decoder. Saved controls are applied only after the audio callback has acknowledged adoption of the selected immutable source.
- When a moved session source corresponds to a local-library record, a successful replacement also reconnects that record on the serialized database worker without editing the music file.
- The Library menu exposes user-visible SQLite backup plus validated restore. Destructive restore is excluded against new search/import/session work, and stale search generations are invalidated before the database snapshot changes.
- Missing/failed sources fail closed, restore waiting is bounded and local library/session data remains local-only with no mandatory cloud/account dependency.

## Gates still open

- M4 still needs realtime-safe deck eject for exact empty-slot restore, playlist/tag editing UI, content-hash indexing, analysis/waveform-cache ownership and real large-library qualification.
- M1 still requires real Windows 11 clean-machine/manual HiDPI/import/device-switching checks and physical master/cue/booth verification.
- Representative music-domain BPM/key/grid evidence, production key-lock listening/latency, controller profiles, long-session soak and physical recording/microphone evidence remain separate gates.

## Next largest step

Implement realtime-safe deck eject for exact empty-slot session restore, with explicit off-callback reclamation and deterministic zero-allocation/deallocation callback evidence. Then continue M4 FINISH FIRST with playlist/tag editing before widening optional DSP scope.
