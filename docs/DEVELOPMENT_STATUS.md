# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `951f6bf5c4597cf9826f4f50696761985b08a552`, the merge commit for PR #69 (`M4: qualify native library backup and restore at scale`). PR #69 exact-head runs `35593343651` (Native library scale smoke) and `35593343285` (Build and test) completed successfully before merge; merged-main runs `35598312597` and `35598312518` also completed successfully.
- Active development: draft PR #70, branch `feat/m4-cache-acceptance-evidence`. Latest functional head before this checkpoint documentation commit: `1ca191638f66c62d5b6ca3daeda4a3b38d293eb8`.
- The functional head hardens the Windows 11 M4 witness so saved evidence is bound to the exact selected `BrokeDJ.exe`: validation now recomputes filename, file version and SHA-256 from `AppPath`, requires Windows 11 x64 evidence, strict JSON booleans for all eight workflow checks, and privacy flags that explicitly remain false.
- M4 witness-tool run `35601511221` completed successfully for that functional head, including negative cases for an incomplete witness, string-boolean spoofing, privacy-unsafe evidence and an executable-fingerprint mismatch. Build-and-test run `35601511287` and Native library scale smoke `35601511437` were still running at this checkpoint and must not be assumed green.
- PR #70 packages the privacy-safe Windows 11 M4 witness recorder plus dedicated Windows CI parsing/evidence-contract checks. It records only OS/app fingerprint data and pass/fail results, never track names, local music paths, screenshots or source music.
- ROADMAP, library and session-persistence documentation on the development branch reflect the already-integrated M4 behavior instead of listing resolved work as merely planned/open.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). M4 remains open until the documented Windows 11 connected-library workflow witness is actually run and reviewed.
- GitHub Releases remains empty; no public BrokeDJ alpha/beta/stable release is qualified by this checkpoint.

## Integrated M4 foundations on main

- Local SQLite schema/migrations, bounded search, native tag editing, ordered playlist membership editing/browsing, playback history, duplicate/missing-file review, moved-file rebinding and backup/restore UI are integrated without modifying original music files.
- Readable imports receive streaming SHA-256 identity outside the realtime callback; relocation refreshes identity and bounded/cancellable maintenance backfills eligible legacy rows.
- Real playback-start edges are recorded on a background worker into local SQLite history and the native PL/EN history view is bounded.
- Windows-native no-audio qualification exercises a deterministic 5,000-track / 12,000-history production-schema fixture, playlist/tag queries, native backup, controlled mutation, native restore and post-restore integrity against both the development and staged EXE.
- Four-deck plus mixer session snapshots are bounded/versioned/checksummed, support explicit verified `.bak` recovery, restore asynchronously while paused, resolve moved sources through the library and have a realtime-safe empty-deck eject path.
- Persistent waveform previews and musical-analysis metadata are source-identity-bound, bounded, generated off the audio thread and fail closed to regeneration when stale or corrupt.
- Library backup validates both live and produced SQLite snapshots. Restore rejects unsupported/future inputs, stages and migrates privately, validates semantic schema/foreign keys and keeps a validated rollback snapshot if live installation fails.

## M4 manual witness package

- `scripts/m4_library_witness.ps1` requires Windows 11, fingerprints the exact BrokeDJ EXE with SHA-256 and records eight user-controlled workflow checks: launch/resize, import/search, tags/playlists, history, duplicate/missing review, relocate, library backup/restore and session save/load.
- Re-validation is now tied to the selected `BrokeDJ.exe` rather than trusting the fingerprint stored inside editable JSON. The validator also rejects non-boolean check values and any evidence whose privacy flags claim paths, track names or source music were captured.
- The script never starts playback automatically and does not inspect or serialize source-music names/paths. The DJ remains in control of any audible playback.
- `docs/M4_LIBRARY_WITNESS.md` defines disposable-test-media guidance, commands, acceptance boundaries and the rule that failed checks remain real blockers rather than being edited into success.
- `.github/workflows/m4-witness-tool.yml` parses the PowerShell script on `windows-latest`, accepts evidence bound to an exact fixture executable and requires intentionally incomplete, type-spoofed, privacy-unsafe and fingerprint-mismatched contracts to fail. This workflow is evidence for the recorder contract, not a substitute for the real manual witness.

## Gates still open

- PR #70 remains draft until Build and test, Native library scale smoke and M4 witness tool are all green for the same final head. This checkpoint changes the head, so fresh exact-final-head checks are required before integration even when the functional-head checks were already green.
- M4 still needs the user-controlled Windows 11 connected workflow witness from the exact EXE being qualified. Automated native fixtures do not substitute for that manual UX/recovery evidence.
- M1 hardware/device switching and four-output cue, M2 representative-music/key-lock/listening, M3 physical microphone/booth/recording/listening, controller profiles and long live soak remain separate gates and do not become satisfied by the M4 witness.

## Next largest step

Require fresh exact-final-head CI for PR #70 after this checkpoint commit. Fix any regression before integration. If every required run is green, keep the package ready for the project integration cadence without advancing `docs/progress.json`; the next M4 state change is evidence-driven by the user-controlled Windows 11 library/session witness and any fixes it exposes. Do not widen the current delivery target with unrelated features while that gate is open.
