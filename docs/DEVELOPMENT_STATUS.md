# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `8cbdb8aea31595e94a0e946e21cd9b21f323ef32`, the merge commit for PR #67 (`M4: add bounded native playlist browsing`). Post-merge workflows `35583279989` (Build and test) and `35583279964` (Native library scale smoke) both completed successfully.
- Active development: draft PR #68, branch `fix/m4-safe-library-restore`. Latest functional head before this checkpoint documentation commit: `c5d38f9bec28d7a540f155f1f090023674008f7d`.
- The package closes a data-safety hole in library restore: a SQLite-valid file could previously claim the current `user_version` while missing BrokeDJ tables, pass `quick_check`, and replace the live snapshot before later code discovered the mismatch.
- Library integrity validation combines SQLite `quick_check`, exact current schema version, required BrokeDJ table/column probes, and `foreign_key_check`.
- Library backup validates the live database before copying and validates the produced backup snapshot before reporting success.
- Library restore copies the selected source into a private temporary database, migrates and validates that staged snapshot before touching the live database, and captures a validated rollback snapshot before installation. A failed installation attempts to restore and revalidate the previous live snapshot.
- Restore now rejects `user_version=0` inputs explicitly. BrokeDJ has no released version-zero backup format, so an arbitrary SQLite database can no longer be treated as a fresh BrokeDJ library and migrated into an empty replacement.
- Deterministic library tests cover malformed current-schema input, a generic SQLite `user_version=0` input with live-snapshot preservation, verified backup re-open/integrity, and v1 staged restore/migration.
- Exact-head workflows for the previous checkpoint `590317db97a59507a87e81d773d824f40be06057` completed successfully (`35588536435` Build and test; `35588536460` Native library scale smoke). This checkpoint changes the head, so those runs are supporting evidence only and new exact-final-head CI is required before integration.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). M4 is materially advancing but is not complete.
- GitHub Releases remains empty; no public BrokeDJ release is qualified by this checkpoint.

## Integrated M4 foundations on main

- Local SQLite schema/migrations, bounded search, native tag editing, ordered playlist membership editing and browsing, playback history, duplicate/missing-file review, moved-file rebinding, and backup/restore UI are integrated without modifying original music files.
- Readable local imports receive streaming SHA-256 content identity outside the realtime callback; relocation refreshes that identity.
- Real playback-start edges are recorded on a background worker into local SQLite history, and the native PL/EN history view is bounded.
- Legacy rows with empty hashes receive bounded/cancellable background maintenance; disconnected paths are marked missing rather than deleted.
- Windows-native no-audio lifecycle qualification exercises a deterministic 5,000-track / 12,000-history production-schema fixture plus playlists/tags against both the development EXE and staged EXE, while keeping audio-device opening disabled.
- Four-deck plus mixer session snapshots are bounded/versioned/checksummed and support explicit verified `.bak` recovery. Restore stays paused, uses asynchronous deck loading and has a realtime-safe empty-deck eject path.
- Persistent waveform previews cache bounded normalized peak data with source size/mtime validation and fail closed to regeneration when stale or corrupt.

## Active M4 safe library restore package

- Semantic schema validation rejects a current-version database that does not actually provide BrokeDJ's required schema.
- Restore accepts only documented BrokeDJ backup schema versions 1 through the current schema version; version 0 and future schema versions fail closed before staged migration or live installation.
- Backup success means both the source and produced SQLite snapshot passed BrokeDJ integrity validation; it does not imply external storage durability or disaster-proof persistence.
- Restore validation/migration happens on a private staged database before the live database is modified.
- A validated rollback snapshot is retained during live installation so an install-time validation failure has a recovery path rather than silently leaving a partially replaced library.
- Database/file work remains outside `MainComponent::getNextAudioBlock()` and this package makes no realtime/audio-quality claim.

## Gates still open

- PR #68 remains development-only until required exact-final-head GitHub Actions checks are green for the same final head.
- Automated native startup/resize/SQL evidence does not replace a user-controlled Windows 11 import/search/tag/playlist/backup/restore interaction witness. M4 remains open until that broader workflow/recovery UX is reviewed.
- Automated builds do not replace Windows 11 clean-machine/manual HiDPI review, physical audio-device switching, master/cue/booth isolation, reviewed listening, controller input or live reliability qualification.
- Representative music-domain BPM/key/grid evidence, production key-lock listening/latency, controller profiles, long-session soak and physical recording/microphone evidence remain separate gates.

## Next largest step

Require green exact-final-head PR #68 build/test and focused native-library checks; fix any regression before integration. If green, mark the PR ready while respecting the project integration cadence, keep the Windows 11 manual M4 library/recovery witness explicit, and finish remaining internally closable M4 acceptance work before widening optional DSP scope.
