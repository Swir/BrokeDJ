# BrokeDJ local library database

This document describes the current M4 database foundation. It is a local persistence layer, not yet a complete user-facing library workflow and not a claim that M4 is complete.

## Dependency and ownership

BrokeDJ uses the official SQLite 3.53.4 amalgamation pinned in `CMakeLists.txt` by the SHA3-256 published on sqlite.org. The SQLite adapter lives in `src/app/`: database/file I/O is never performed by `Engine::process()` and the audio core remains independent from SQLite.

The database is local-only. It contains local music paths because a DJ library must be able to find the user's files, but BrokeDJ does not upload them, place them in public diagnostics or require an account/cloud service.

## Schema and migrations

`LibraryDatabase::currentSchemaVersion` is currently **2**.

- schema 1: tracks, tags, track/tag relations, playlists, ordered playlist items and play history;
- schema 2: content hashes, missing-file state and the first search/history/duplicate indexes.

Migrations are transactional and ordered. Opening a database with a schema newer than this BrokeDJ build fails closed instead of guessing how to downgrade it. A version-1 fixture is migrated in the deterministic test suite.

## Current capabilities

The adapter provides stable track upsert, bounded local search, case-insensitive tags, ordered playlists, play history, content-hash duplicate grouping, explicit missing-file state, moved-file rebinding that preserves relationships, SQLite online backup/restore and `quick_check` validation. Backup retry work is bounded. All filesystem/database work belongs on the UI/worker side, never in the realtime audio callback.

## Test evidence

`tests/LibraryDatabaseTests.cpp` uses temporary databases only. It covers fresh schema creation, v1→v2 migration, future-schema rejection, search/tags/playlists/history, duplicate grouping, moved-file rebinding, backup/restore, corrupt-backup rejection and future-backup rejection without replacing the live database. The tests never touch or alter the user's music.

## Still open for M4

This database foundation intentionally does not close M4. The next finish-first work is to connect the adapter to the native BrokeDJ library UI and import workflow, then add real analysis/waveform-cache ownership, session persistence and user-visible backup/restore. Duplicate hashes must be produced off the audio thread by the import/indexing worker; this layer only stores and queries them.
