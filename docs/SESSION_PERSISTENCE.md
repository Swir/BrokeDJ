# BrokeDJ session persistence

This document describes the current M4 local session-snapshot and native restore workflow. It is not a claim that M4, release qualification or live-performance qualification is complete.

## Scope

`src/app/SessionStore.h` stores a bounded snapshot for the four BrokeDJ decks plus mixer state. The snapshot contains each deck's local source path, transport position, playback rate, trim, channel gain, three-band EQ, echo, drive, headphone-cue state, whole-track-loop state and whether the deck was playing when captured. Mixer state stores crossfader, master level and headphone level.

The native Library menu exposes Save Session, Load Session and explicit verified-backup recovery. Session save/load runs on the bounded application worker, never in `MainComponent::getNextAudioBlock()`. Session files are BrokeDJ-owned local files; the store never edits source music and no cloud/account service is required.

## Native restore safety

Loading a valid snapshot first pauses all four current transports and restores mixer state. Non-empty sources are decoded through BrokeDJ's normal asynchronous loader. Saved transport/EQ/FX/fader/cue/loop controls are applied only after the expected file is actually loaded and adopted by the audio engine; a failed decode cannot apply saved controls to the wrong clip.

The restore coordinator does not infer adoption from a path match or an old duration meter. After the expected source is published, it arms a zero-position `Controls::seek` marker. `Engine::process()` adopts pending clips before consuming that atomic seek mailbox, so observing the marker consumed acknowledges that an audio callback has passed the adoption point for that publication. Only then can saved controls and position be applied. If callbacks are unavailable, the marker remains unconsumed and restore fails boundedly instead of mutating stale audio state.

Saved `wasPlaying` state is intentionally **not** auto-resumed. Every restored deck remains paused until the DJ explicitly presses Play. Empty saved slots use the realtime-safe deck-eject path so an older source cannot remain logically loaded in a session slot that should be empty.

Missing saved paths are resolved against BrokeDJ's local library metadata before restore is abandoned, allowing a previously relocated library record to rebind a saved session without rewriting the session file or original music. A source that still cannot be resolved fails closed and is counted in the restore report.

## Format and recovery contract

Session schema version 1 uses a compact binary envelope with an explicit magic value, schema version, payload length and 64-bit FNV-1a checksum. The reader rejects truncated data, checksum mismatches, unsupported flag bits, non-finite/out-of-range controls, oversized paths and files created by a newer schema.

Writes are bounded to a 1 MiB session file and 64 KiB per local path. Saving writes a temporary file first, reads it back through the same validator, preserves an existing session as a temporary backup, publishes the verified file by rename and attempts rollback if publication fails. A rejected or invalid new snapshot must not destroy the last valid session.

Backup recovery is explicit rather than silent. `load()` accepts only the requested primary snapshot. The native Recover command uses `loadRecoveringBackup()` and reports when verified `.bak` recovery was used. Recovery never starts audio or silently rewrites the damaged primary file.

## Test and CI evidence

`tests/LibraryDatabaseTests.cpp` exercises deterministic session save/load alongside the local-library persistence suite. Coverage includes four-deck independence, mixer/control round-trip, non-finite-state rejection without replacing the previous valid session, future-schema rejection, checksum-corruption detection, oversized-path rejection, missing-primary backup recovery and corrupt-primary backup recovery.

The native session workflow is additionally gated by BrokeDJ's Windows no-audio GUI construction/resize smoke because `RecordingMainComponent` constructs the Library/Session workflow. Exact-head project CI also covers Linux ASan/UBSan, full Windows CTest, audio diagnostics, silent device probe, staging and downloaded-artifact smoke before integration.

M4's connected user workflow is separately captured by `M4_LIBRARY_WITNESS.md`: save a session, alter state, load it on Windows 11 and verify restored controls while decks remain paused until explicit Play. The witness also covers the surrounding library operations that session relocation/recovery depends on.

## Remaining M4 gate

Realtime-safe empty-deck ejection, moved-file resolution, playlist/tag editing, content-hash indexing/backfill, waveform/analysis-cache ownership, user-visible library backup/restore and deterministic large-library/native recovery qualification are integrated. The remaining explicit M4 acceptance gate is the documented user-controlled Windows 11 library/session witness on the exact BrokeDJ EXE being qualified, plus fixing any regression that witness exposes. Physical audio routing, controller and broader listening/live-soak evidence belong to their separate milestones and are not implied by M4 completion.
