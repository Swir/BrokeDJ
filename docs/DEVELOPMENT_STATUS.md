# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `6f8afd22ae998fb1f6db62e96e0f8ddd6262da1b`, the merge commit for PR #70 (`M4: package the Windows library workflow witness`).
- PR #70 exact-final-head `6f12237c84a68cf1919fac6f224b4726e8d7f56a` passed all three required pull-request workflows before merge: M4 witness tool `35601862448`, Native library scale smoke `35601862697` and Build and test `35601862558`.
- Merged-main verification for `6f8afd22ae998fb1f6db62e96e0f8ddd6262da1b` is green: Build and test `35604132889` and Native library scale smoke `35604132898` completed successfully.
- Active development: draft PR #71, branch `feat/m4-self-contained-witness-package`. Earlier exact head `bd3131c9bdb1beee7bb41f1e0d981799df3e842f` passed all three pull-request workflows: Native library scale smoke `35604773637`, M4 witness tool `35604773640` and Build and test `35604773488`.
- This run tightened the M4 witness evidence boundary: `scripts/m4_library_witness.ps1` now requires exact property sets at every object level, type-checks schema/Windows-build integers and identity strings, retains strict booleans, and rejects unexpected free-form/private fields while still binding evidence to the exact selected `BrokeDJ.exe` filename/version/SHA-256.
- `.github/workflows/m4-witness-tool.yml` adds negative cases for string-typed schema/build values plus unexpected private and top-level fields, in addition to incomplete, boolean-spoofed, privacy-unsafe and executable-fingerprint mismatch cases.
- The first closed-schema attempt exposed a real PowerShell 7 compatibility regression in run `35610932444`: `ConvertFrom-Json` can materialize an ISO-8601 timestamp as `DateTime` instead of leaving it as a string. The branch was not merged. Functional fix head `e72702a0ddbb46c25fd32aa2d432963f89c0ffa9` accepts PowerShell's timestamp materialization while preserving strict validation for security/privacy-relevant fields.
- On functional fix head `e72702a0ddbb46c25fd32aa2d432963f89c0ffa9`, M4 witness-tool run `35611079000` completed successfully. Build and test `35611079025` and Native library scale smoke `35611079798` were still in progress at this checkpoint.
- This checkpoint commit changes the PR head again, so fresh exact-final-head checks are required before integration even if the functional-head checks later finish green.
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
- The development branch validates a closed evidence schema: no extra properties are accepted in the evidence/environment/app/checks/privacy objects, schema/build and identity fields are type-checked, and the exact EXE fingerprint remains mandatory. Timestamp validation accounts for PowerShell 7 materialization without relaxing other field types.
- `docs/M4_LIBRARY_WITNESS.md` defines disposable-test-media guidance, commands, the closed evidence boundary, acceptance boundaries and the rule that failed checks remain real blockers rather than being edited into success.
- `.github/workflows/m4-witness-tool.yml` validates the evidence contract on `windows-latest`, including incomplete, type-spoofed, privacy-unsafe, unexpected-field and executable-fingerprint-mismatched negative cases. This workflow is evidence for the recorder contract, not a substitute for the real manual witness.
- PR #71 places the recorder and guide beside the staged EXE and adds downloaded-artifact presence checks so the user can run the witness from one integrity-covered directory.

## Gates still open

- Fresh exact-final-head CI is required for PR #71 after this checkpoint commit. Fix any regression before merge; do not reuse an earlier green head as final-head evidence.
- M4 still needs the user-controlled Windows 11 connected workflow witness from the exact EXE being qualified. Automated native fixtures and CI tooling do not substitute for that manual UX/recovery evidence.
- M1 hardware/device switching and four-output cue, M2 representative-music/key-lock/listening, M3 physical microphone/booth/recording/listening, controller profiles and long live soak remain separate gates and do not become satisfied by the M4 witness.

## Next largest step

Require fresh exact-final-head Build and test, Native library scale smoke and M4 witness-tool CI for PR #71. Repair any regression before integration. Keep #71 draft under the normal integration cadence; this branch removes source-checkout friction and rejects schema drift/private free-form evidence while the externally controlled Windows 11 M4 witness remains outstanding. Do not advance `docs/progress.json` until that witness is real and reviewed.
