# BrokeDJ session persistence

This document describes the current M4 local session-snapshot and native restore workflow. It is not a claim that M4, release qualification or live-performance qualification is complete.

## Scope

`src/app/SessionStore.h` stores a bounded snapshot for the four BrokeDJ decks plus mixer state. The snapshot contains each deck's local source path, transport position, playback rate, trim, channel gain, three-band EQ, echo, drive, headphone-cue state, whole-track-loop state and whether the deck was playing when captured. Mixer state stores crossfader, master level and headphone level.

The native Library menu now exposes Save Session, Load Session and explicit verified-backup recovery. Session save/load runs on the existing bounded application worker, never in `MainComponent::getNextAudioBlock()`. The store never edits or overwrites source music. Session files are separate BrokeDJ-owned files and no cloud/account service is required.

## Native restore safety

Loading a valid snapshot first pauses all four current transports and restores mixer state. Existing track paths are then decoded through BrokeDJ's normal asynchronous loader. Saved transport/EQ/FX/fader/cue/loop controls are applied only after the expected file is actually loaded and adopted by the audio engine; a failed decode cannot apply saved controls to the wrong clip.

Saved `wasPlaying` state is intentionally **not** auto-resumed. Every restored deck remains paused until the DJ explicitly presses Play. This avoids surprise audio when opening a session. Missing source files fail closed and are counted in the restore report. A restore waiting for clip adoption is bounded; an unavailable audio path cannot leave the UI waiting forever or be reported as a successful deck restore.

Current pre-alpha limitation: an empty saved deck slot does not yet eject a source that was already loaded in that slot before restore; the old source is paused and the completion dialog states this limitation. A true realtime-safe deck-eject command remains open M4 work.

## Format and recovery contract

Session schema version 1 uses a compact binary envelope with an explicit magic value, schema version, payload length and 64-bit FNV-1a checksum. The reader rejects truncated data, checksum mismatches, unsupported flag bits, non-finite/out-of-range controls, oversized paths and files created by a newer schema.

Writes are bounded to a 1 MiB session file and 64 KiB per local path. Saving writes a temporary file first, reads it back through the same validator, preserves an existing session as a temporary backup, publishes the verified file by rename and attempts rollback if publication fails. A rejected or invalid new snapshot must not destroy the last valid session.

Backup recovery is explicit rather than silent. `load()` accepts only the requested primary snapshot. The native Recover command uses `loadRecoveringBackup()` and reports when verified `.bak` recovery was used. Recovery never starts audio or silently rewrites the damaged primary file.

## Test and CI evidence

`tests/LibraryDatabaseTests.cpp` exercises deterministic session save/load alongside the local-library persistence suite. Coverage includes four-deck independence, mixer/control round-trip, NaN rejection without replacing the previous valid session, future-schema rejection, checksum-corruption detection, oversized-path rejection, missing-primary backup recovery and corrupt-primary backup recovery.

The native session workflow is additionally gated by BrokeDJ's Windows no-audio GUI construction/resize smoke because `RecordingMainComponent` constructs the Library/Session workflow. The exact PR head must also pass Linux ASan/UBSan, full Windows CTest, audio diagnostics, silent device probe, staging and downloaded-artifact smoke before merge.

## Still open for M4

M4 still needs realtime-safe deck eject for truly empty restored slots, moved-file resolution during session restore, playlist/tag editing UI, content-hash indexing, analysis/waveform-cache ownership, richer session state where justified, user-visible library backup/restore and real large-library qualification. Restored sessions must continue to fail closed and never modify original music.
