# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `6f8afd22ae998fb1f6db62e96e0f8ddd6262da1b`, the merge commit for PR #70 (`M4: package the Windows library workflow witness`).
- PR #70 exact-final-head `6f12237c84a68cf1919fac6f224b4726e8d7f56a` passed all three required pull-request workflows before merge: M4 witness tool `35601862448`, Native library scale smoke `35601862697` and Build and test `35601862558`.
- Merged-main verification for `6f8afd22ae998fb1f6db62e96e0f8ddd6262da1b` started as Build and test `35604132889` and Native library scale smoke `35604132898`; both were still running at this checkpoint and must not be assumed green yet.
- Active development: branch `feat/m4-self-contained-witness-package`. The package workflow now stages `M4-LIBRARY-WITNESS.ps1` and `M4-LIBRARY-WITNESS.md` directly beside the exact staged `BrokeDJ.exe`, so the remaining user-controlled M4 witness no longer requires a separate source checkout. The existing package manifest/checksum tool covers those files automatically, and the downloaded-artifact smoke rejects a package if either witness file is absent.
- `docs/M4_LIBRARY_WITNESS.md` now documents the one-directory staged-artifact command while preserving the source-checkout path and the exact-EXE SHA-256 validation boundary.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). M4 remains open until the documented Windows 11 connected-library/session witness is actually run and reviewed.
- GitHub Releases remains empty; no public BrokeDJ alpha/beta/stable release is qualified by this checkpoint.

## Integrated M4 foundations on main

- Local SQLite schema/migrations, bounded search, native tag editing, ordered playlist membership editing/browsing, playback history, duplicate/missing-file review, moved-file rebinding and backup/restore UI are integrated without modifying original music files.
- Readable imports receive streaming SHA-256 identity outside the realtime callback; relocation refreshes identity and bounded/cancellable maintenance backfills eligible legacy rows.
- Real playback-start edges are recorded on a background worker into local SQLite history and the native PL/EN history view is bounded.
- Windows-native no-audio qualification exercises a deterministic 5,000-track / 12,000-history production-schema fixture, playlist/tag queries, native backup, controlled mutation, native restore and post-restore integrity against both the development and staged EXE.
- Four-deck plus mixer session snapshots are bounded/versioned/checksummed, support explicit verified `.bak` recovery, restore asynchronously while paused, resolve moved sources through the library and have a realtime-safe empty-deck eject path.
- Persistent waveform previews and musical-analysis metadata are source-identity-bound, bounded, generated off the audio thread and fail closed to regeneration when stale or corrupt.
- Library backup validates both live and produced SQLite snapshots. Restore rejects unsupported/future inputs, stages and migrates privately, validates semantic schema/foreign keys and keeps a validated rollback snapshot if live installation fails.
- The Windows 11 M4 witness recorder is bound to the exact selected `BrokeDJ.exe`: validation recomputes filename, file version and SHA-256, requires Windows 11 x64 evidence, strict JSON booleans for all eight workflow checks and privacy flags that remain false.

## M4 manual witness package

- `scripts/m4_library_witness.ps1` requires Windows 11 and records eight user-controlled workflow checks: launch/resize, import/search, tags/playlists, history, duplicate/missing review, relocate, library backup/restore and session save/load.
- The script never starts playback automatically and does not inspect or serialize source-music names/paths. The DJ remains in control of any audible playback.
- `docs/M4_LIBRARY_WITNESS.md` defines disposable-test-media guidance, commands, acceptance boundaries and the rule that failed checks remain real blockers rather than being edited into success.
- `.github/workflows/m4-witness-tool.yml` validates the evidence contract on `windows-latest`, including incomplete, type-spoofed, privacy-unsafe and executable-fingerprint-mismatched negative cases. This workflow is evidence for the recorder contract, not a substitute for the real manual witness.
- The current development package change places the recorder and guide beside the staged EXE and adds downloaded-artifact presence checks so the user can run the witness from one integrity-covered directory.

## Gates still open

- Exact-head CI is required for the current self-contained witness-package branch before it can be considered for integration. Fix any regression before merge.
- M4 still needs the user-controlled Windows 11 connected workflow witness from the exact EXE being qualified. Automated native fixtures and CI tooling do not substitute for that manual UX/recovery evidence.
- M1 hardware/device switching and four-output cue, M2 representative-music/key-lock/listening, M3 physical microphone/booth/recording/listening, controller profiles and long live soak remain separate gates and do not become satisfied by the M4 witness.

## Next largest step

Open one draft PR for `feat/m4-self-contained-witness-package`, require exact-head Build and test plus the package smoke path, and repair any regression before integration. Because PR #70 has just merged, do not force another default-branch merge merely for hourly cadence; this branch exists to make the externally blocked M4 witness materially easier to run. M4 progress remains unchanged until the real Windows 11 witness is completed and reviewed.
