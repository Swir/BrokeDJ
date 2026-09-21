# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch code baseline: `main` at `f2ef11cf6749ce95140fe38f66fbf2b11a031c5d`; PR #59 (`M4: recover moved session sources and expose library backups`) is integrated on `main`.
- Active development branch: `feat/m4-realtime-deck-eject`, PR #60 (`M4: restore empty session slots with realtime-safe deck eject`). Implementation head `9af14235a139e3d197f9d69ff9225a4fe8ef029f` started exact-head workflow `35550577449`; this documentation checkpoint follows that implementation and therefore requires a new exact-final-head workflow before merge.
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

## PR #60 development scope

- Core `Engine::eject()` now requests source removal through the same serialized non-audio ownership boundary used for clip publication. The audio callback only moves the active immutable `Clip` to a retired slot; deletion remains in the existing non-audio `collectRetired()` path.
- A pending but not-yet-adopted clip can be cancelled and destroyed on the serialized non-audio publisher before the clear command is consumed. A later publication supersedes an unconsumed clear deterministically.
- Exact session restore now treats saved empty deck paths as real empty slots. It stops/disarms transport ownership, invalidates stale analysis/hotcue/grid work, requests realtime-safe source removal, waits for zero-duration audio ownership, clears deck metadata/UI and restores the saved channel controls without auto-play.
- An in-flight asynchronous decode causes empty-slot restore to fail closed rather than allowing a late decoder publication to repopulate the deck after the clear request.
- `RealtimeContractTests` now exercises active-source eject, pending-source cancellation, repopulation after eject, zero transport/duration/meters and the requirement that the callback performs zero heap allocations and zero heap deallocations during clear adoption.

## Gates still open

- PR #60 is not integrated until its exact-final-head Linux sanitizer/CTest and Windows x64 build/test/no-audio GUI/device/package controls are green. It must also respect the repository integration cadence instead of merging only because an hourly work slice exists.
- M4 still needs playlist/tag editing UI, content-hash indexing, analysis/waveform-cache ownership and real large-library qualification after the deck-eject slice is integrated.
- M1 still requires real Windows 11 clean-machine/manual HiDPI/import/device-switching checks and physical master/cue/booth verification.
- Representative music-domain BPM/key/grid evidence, production key-lock listening/latency, controller profiles, long-session soak and physical recording/microphone evidence remain separate gates.

## Next largest step

First qualify PR #60 on its exact final head and fix any regression before integration. Then continue M4 FINISH FIRST with playlist/tag editing, followed by content-hash indexing and analysis/waveform-cache ownership before widening optional DSP scope.
