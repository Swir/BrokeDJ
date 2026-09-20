# BrokeDJ session persistence

This document describes the current M4 session-snapshot foundation. It is local-only persistence and is not yet a complete user-facing Save/Load Session workflow.

## Scope

`src/app/SessionStore.h` stores a bounded snapshot for the four BrokeDJ decks plus mixer state. The snapshot currently contains each deck's local source path, transport position, playback rate, trim, channel gain, three-band EQ, echo, drive, headphone-cue state, whole-track-loop state and whether the deck was playing when captured. Mixer state stores crossfader, master level and headphone level.

The store never edits or overwrites source music. Session files are separate BrokeDJ-owned files and all session filesystem work belongs outside the realtime audio callback.

## Format and recovery contract

Session schema version 1 uses a compact binary envelope with an explicit magic value, schema version, payload length and 64-bit FNV-1a checksum. The reader rejects truncated data, checksum mismatches, unsupported flag bits, non-finite/out-of-range controls, oversized paths and files created by a newer schema.

Writes are bounded to a 1 MiB session file and 64 KiB per local path. Saving writes a temporary file first, reads it back through the same validator, preserves an existing session as a temporary backup, publishes the verified file by rename and attempts rollback if publication fails. A rejected or invalid new snapshot must not destroy the last valid session.

`wasPlaying` is persisted only as session history/state. Future native restore wiring must not silently auto-start audio merely because a saved deck was playing; resuming playback must remain an explicit product decision with safe user-visible behavior.

## Test evidence

`tests/LibraryDatabaseTests.cpp` exercises deterministic session save/load alongside the local-library persistence suite. Coverage includes four-deck independence, mixer/control round-trip, NaN rejection without replacing the previous valid session, future-schema rejection, checksum-corruption detection and oversized-path rejection. The header also passes a strict standalone C++20 compile with warnings promoted to errors in the development checkpoint.

## Still open for M4

The native application does not yet expose Save Session / Load Session commands and does not yet reconnect restored paths through the library/missing-file workflow. The next finish-first work is to wire session capture/restore into the native UI and connect the existing local library database to the import/search workflow. Missing or moved tracks must fail closed and remain user-resolvable; restored sessions must never modify original music.
