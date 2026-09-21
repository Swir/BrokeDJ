# BrokeDJ local library

BrokeDJ's library is local-only. SQLite metadata, tags, playlists, history, content identities and waveform/analysis cache data stay on the machine unless the user explicitly creates a backup. BrokeDJ never deletes or overwrites original music as part of duplicate, missing-file, relocation or recovery workflows.

## Duplicate and missing-file review

The native library search supports two exact review directives:

- `is:duplicate` — show non-missing tracks whose populated content hash is shared by another non-missing library record.
- `is:missing` — show records whose source file has been marked missing so they can be reconnected with **Relocate**.

Duplicate review is intentionally non-destructive. Matching records are candidates for the DJ to inspect; BrokeDJ does not auto-delete, merge or rewrite source audio.

## Content identity and maintenance

When an imported local track reaches SQLite without an existing content identity, the database adapter computes a streaming SHA-256 over the file outside the realtime audio callback. The file is read in bounded chunks rather than loaded wholly into RAM. Relocating a record recomputes identity for the selected replacement file before the new path is committed, so stale duplicate matches do not survive a content change.

Older migrated rows may have an empty hash. A bounded, cancellable background maintenance path can fill eligible local rows without blocking playback or database open. Disconnected paths are marked missing instead of being deleted, and unavailable files remain excluded from duplicate matching until they are reconnected.

## Cache ownership

Waveform previews and musical-analysis records are persistent local cache data, not source music. Cache generation and reads that touch disk stay outside the realtime callback. Waveform records store bounded normalized peak data and validate source size/modification identity; stale or corrupt cache entries fail closed and are regenerated. Analysis cache and user-reviewed beat-grid state are also source-identity-bound so replaced files do not silently inherit unrelated analysis.

## Backup and recovery

Library backup validates both the live SQLite source and the produced snapshot. Restore stages the candidate privately, rejects unsupported/future schemas, migrates supported older BrokeDJ schemas before installation, validates semantic tables/foreign keys/integrity and retains a validated rollback snapshot if live replacement fails.

The Windows-native recovery harness exercises the production C++ backup/restore paths without constructing the normal app window or opening audio. Its deterministic fixture contains 5,000 tracks and 12,000 history entries plus playlist/tag relationships. It creates a real native backup, mutates independent library state, restores it and requires the original cardinalities/relationships plus `PRAGMA quick_check=ok` to return for both the build-tree and freshly staged EXE. Synthetic fixture paths are nonexistent and no source music is uploaded as CI evidence.

## Qualification limits

Automated tests and Windows-native CI establish repeatable library/recovery behavior, but they are not a maximum-library-size, storage-latency or interactive-UX certification. M4 still requires the user-controlled Windows 11 workflow witness documented in `M4_LIBRARY_WITNESS.md`, covering launch/resize, import/search, tags/playlists, history, duplicate/missing review, relocate, library backup/restore and session save/load on the exact EXE being qualified. Network/removable storage and long live sessions remain separate later qualification work unless explicitly required by another milestone.
