# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `7ad930d5cec67bab6c1e8e348174b9a3510cf1e7`, the merge commit for PR #68 (`M4: make library restore fail-safe and schema-aware`). PR #68 exact-head runs `35590017375` (Build and test) and `35590017370` (Native library scale smoke) completed successfully before merge.
- Post-merge `main` Native library scale smoke `35592377998` completed successfully. Build and test `35592377896` was still queued at this checkpoint and must be reported separately rather than assumed green.
- Active development: draft PR #69, branch `feat/m4-native-library-recovery-smoke`. Functional head before this checkpoint documentation commit: `e36035cf96e2a222783d5ce04bca5c79795af31d`.
- PR #69 adds guarded no-audio `--library-backup-smoke` and `--library-restore-smoke` application paths. They exercise the production `LibraryDatabase` implementation against BrokeDJ's normal local database location without constructing the app window, opening an audio device or starting an audio callback.
- Recovery smoke is restricted to GitHub Actions or the explicit `BROKEDJ_ALLOW_LOCAL_LIBRARY_RECOVERY_SMOKE=1` fixture override. The normal large-library harness still refuses destructive local seeding unless `--allow-local` is supplied.
- The 5,000-track / 12,000-history fixture now performs a native backup, makes a controlled SQLite mutation (title, one playlist item and one history row), runs native restore, then requires full fixture cardinality/search relationships and `PRAGMA quick_check=ok` to be restored before the existing native GUI lifecycle check.
- Both development and freshly staged EXEs are gated by the same schema-v3 recovery contract. Workflow artifacts retain JSON evidence but do not upload the fixture database or backup snapshot.
- `scripts/library_scale_smoke.py` passed a local Python syntax compilation check after the recovery extension. Exact-final-head Windows/Linux CI is still required; local syntax validation is not a substitute for it.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). M4 is materially advancing but is not complete.
- GitHub Releases remains empty; no public BrokeDJ release is qualified by this checkpoint.

## Integrated M4 foundations on main

- Local SQLite schema/migrations, bounded search, native tag editing, ordered playlist membership editing and browsing, playback history, duplicate/missing-file review, moved-file rebinding, and backup/restore UI are integrated without modifying original music files.
- Readable local imports receive streaming SHA-256 content identity outside the realtime callback; relocation refreshes that identity.
- Real playback-start edges are recorded on a background worker into local SQLite history, and the native PL/EN history view is bounded.
- Legacy rows with empty hashes receive bounded/cancellable background maintenance; disconnected paths are marked missing rather than deleted.
- Windows-native no-audio lifecycle qualification exercises a deterministic 5,000-track / 12,000-history production-schema fixture plus playlists/tags against both the development EXE and staged EXE while keeping audio-device opening disabled.
- Four-deck plus mixer session snapshots are bounded/versioned/checksummed and support explicit verified `.bak` recovery. Restore stays paused, uses asynchronous deck loading and has a realtime-safe empty-deck eject path.
- Persistent waveform previews cache bounded normalized peak data with source size/mtime validation and fail closed to regeneration when stale or corrupt.
- Library backup validates both the live source and produced SQLite snapshot. Restore rejects undocumented version-zero/future inputs, stages and migrates privately, validates semantic schema/foreign keys, and retains a validated rollback snapshot for failed live installation.

## Active M4 native recovery qualification package

- The native backup smoke creates a fresh recovery snapshot through the production C++ `LibraryDatabase::backupTo()` path and requires a non-empty validated artifact.
- The fixture then proves that recovery is meaningful by modifying three independent pieces of library state before invoking the production C++ `restoreFrom()` path.
- Post-restore validation requires the original track title, playlist membership, history cardinality, full 5k/12k data set, playlist/tag query contract and SQLite integrity to return exactly to the seeded state.
- The same flow runs against the build-tree EXE and the freshly staged EXE, so packaging cannot silently drop the recovery behavior.
- All fixture paths are synthetic and deliberately nonexistent. No source music is created, read for playback, modified or deleted by this qualification flow.

## Gates still open

- PR #69 remains draft until required exact-final-head Build and test plus Native library scale smoke workflows are green for the same final head. Any CI regression is fixed before integration.
- Automated native recovery evidence does not replace a user-controlled Windows 11 import/search/tag/playlist/backup/restore interaction witness. M4 remains open until that broader workflow/recovery UX is reviewed.
- Automated builds do not replace Windows 11 clean-machine/manual HiDPI review, physical audio-device switching, master/cue/booth isolation, reviewed listening, controller input or live reliability qualification.
- Representative music-domain BPM/key/grid evidence, production key-lock listening/latency, controller profiles, long-session soak and physical recording/microphone evidence remain separate gates.

## Next largest step

Require fresh exact-final-head CI for PR #69 after this checkpoint commit. Fix any build, staged-package or recovery-contract regression before marking the PR ready. If green, integrate the coherent recovery package without claiming M4 complete, then finish any remaining internally automatable M4 acceptance evidence before treating the Windows 11 manual library/recovery witness as the explicit external gate.
