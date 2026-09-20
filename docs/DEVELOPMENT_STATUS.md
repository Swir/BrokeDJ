# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch code baseline: `main` at status commit `27d5889429649df737798ee97640bd9ad16857f7`; the latest verified feature merge underneath it is PR #57 at `0a353208db4782ec4345c443c909e6b39d8ecff4`.
- PR #55 (`M4: add bounded session persistence foundation`) is merged after exact-final-head workflow `35539900396` succeeded on Linux ASan/UBSan, Windows x64 build/full CTest/audio diagnostics/native no-audio GUI smoke/device probe, staging and downloaded-artifact smoke.
- PR #56 (`M4: connect native local library workflow`) is merged after exact-final-head workflow `35540530581` succeeded on the same Linux/Windows/package gates, including downloaded staged-artifact verification and no-audio smoke.
- PR #57 (`M4: add native Save Load Session workflow`) is merged after exact-final-head workflow `35541843965` succeeded on Linux ASan/UBSan, Windows x64 build/full CTest/audio diagnostics/native no-audio GUI smoke/device probe, staging/upload and downloaded staged-artifact smoke.
- Development branch `feat/m4-session-recovery-maintenance` / PR #59 continues FINISH FIRST. Functional code checkpoint `e36e8c81ba2f9f6f86fc083932d1fd7434e01b36` adds guided moved-file session recovery and native library backup/restore controls. Exact-head workflow `35544237152` was started for that code checkpoint; the PR must not merge until the final PR head has passed the required Linux/Windows/package checks.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). These M4 foundations do not close M4.
- GitHub Releases remains empty; no public BrokeDJ Release is qualified by this checkpoint.

## Integrated M4 foundations on main

- The local SQLite library foundation provides schema migrations, bounded search, tags, ordered playlists, play history, duplicate grouping, missing/moved-file rebinding, integrity checks and backup/restore without modifying original music.
- The session foundation provides bounded/versioned four-deck plus mixer snapshots, checksum/range validation, verified temporary publication, rollback handling and explicit verified backup recovery. Session I/O remains outside the realtime callback.
- The native PL/EN local-library workflow provides bounded type-ahead search, multi-file import, explicit target-deck loading and moved-file relocation. Search and writes use separate bounded single-worker paths with shutdown cancellation.
- Native Save Session / Load Session / verified `.bak` recovery is wired into the application. Restore pauses all transports, loads sources through the normal async decoder and does not auto-resume saved `wasPlaying` state.
- Restored controls and position are applied only after an audio-thread adoption acknowledgement: the expected source is published, a zero-position seek marker is armed, and state is applied only after `Engine::process()` consumes that marker after its clip-adoption point. A stale previous-clip duration/path match cannot satisfy restore early.
- Missing/failed sources fail closed, restore waiting is bounded and local library/session data remains local-only with no cloud/account dependency.

## PR #59 development slice

- A missing path inside a saved session no longer has to be abandoned immediately: the native PL/EN restore workflow offers a bounded per-deck file chooser, feeds the selected replacement through the existing asynchronous decoder and retains the same audio-thread adoption acknowledgement before saved deck controls are applied.
- When the old session path corresponds to a local-library record, a successful replacement selection also reconnects that record to the moved file on the serialized database worker, preserving its library identity/relationships without editing the music file itself.
- The Library menu now exposes user-visible SQLite backup and validated restore commands. Destructive restore is excluded against new search/import/session work at workflow level and stale type-ahead generations are invalidated before the database snapshot changes.
- These changes are development-branch work until exact-final-head CI passes and PR #59 is merged.

## Gates still open

- M4 still needs realtime-safe deck eject for exact empty-slot restore, playlist/tag editing UI, content-hash indexing, analysis/waveform-cache ownership and real large-library qualification. Moved-file session recovery plus user-visible library backup/restore are being closed by PR #59, subject to final CI/merge.
- M1 still requires real Windows 11 clean-machine/manual HiDPI/import/device-switching checks and physical master/cue/booth verification.
- Representative music-domain BPM/key/grid evidence, production key-lock listening/latency, controller profiles, long-session soak and physical recording/microphone evidence remain separate gates.

## Next largest step

Finish PR #59 only after exact-final-head checks are green, then implement realtime-safe deck eject for exact empty-slot session restore before widening optional DSP scope. Playlist/tag editing follows that closure.
