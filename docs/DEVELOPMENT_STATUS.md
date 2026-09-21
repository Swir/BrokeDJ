# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `0a950c4db4498fd8a4098314e1a54177de3fe1a6`. The merged M4 waveform-cache package passed main workflow `35557872571` across Linux sanitizer/CTest and Windows x64 build/full CTest/no-audio GUI/device/package checks.
- Active development: PR #63, branch `feat/m4-content-hash-review`. Functional package head before this checkpoint-only documentation commit: `3966efcd1d75d3ce3a18a3153e144432aaa13026`. Exact-head CI for the resulting PR head is required before merge; no run had been assigned when this checkpoint text was written.
- This package populates content hashes for readable local imports, refreshes hashes on relocation, exposes non-destructive duplicate/missing review through the native library search, and adds deterministic synthetic-library qualification.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). M4 is materially advancing but is not complete.
- GitHub Releases remains empty; no public BrokeDJ release is qualified by this checkpoint.

## Integrated M4 foundations on main

- Local SQLite schema/migrations, bounded search, native tag editing, ordered playlist membership editing, play-history storage, duplicate grouping, missing/moved-file rebinding, integrity checking and backup/restore are integrated without modifying original music files.
- Four-deck plus mixer session snapshots are bounded/versioned/checksummed, support verified backup recovery, and keep session I/O outside the realtime callback.
- The native PL/EN local-library workflow provides bounded type-ahead search, multi-file import, explicit target-deck loading and moved-file relocation on background workers.
- Native Save Session / Load Session / verified `.bak` recovery pauses transports, uses the normal asynchronous decoder, reconnects moved library records when possible and does not auto-resume saved playback.
- Empty saved session slots use the realtime-safe deck-eject mailbox: the audio callback never deletes the retired clip and deterministic realtime tests require zero heap allocations/deallocations during clear adoption.
- Persistent waveform previews cache only bounded normalized peak data with source size/mtime validation. Cache I/O stays in decoder/background work and stale/corrupt entries fail closed to regeneration.

## Active M4 content-identity package

- `LibraryDatabase::upsertTrack()` now fills an empty `content_hash` from the readable local file using a streaming SHA-256 implementation. Callers that already supply a content identity keep their supplied value, preserving migration/test compatibility.
- Relocation recomputes the selected file's hash before publishing the new path, preventing stale duplicate identity after a user reconnects a moved record to different content.
- The existing native library search accepts `is:duplicate` for non-missing tracks that share a populated content hash and `is:missing` for disconnected records. These are review filters only: BrokeDJ does not delete, merge or overwrite source music.
- Deterministic coverage adds the SHA-256 `abc` reference vector, automatic duplicate discovery, relocation rehash invalidation, missing-track filtering and a synthetic 1,500-row database/query diagnostic. Shared-runner timing is diagnostic only, not a performance guarantee.
- Local host compilation of the database package with GCC 14 and system SQLite passed the focused content-hash/duplicate/relocation fixture before publication. Repository CI remains the authoritative cross-platform gate for this branch.

## Gates still open

- This branch is development-only until exact-final-head GitHub Actions is green. Automated checks do not replace physical Windows 11 audio-device qualification or reviewed listening.
- M4 still needs real playback-history wiring/review UX and broader large-library/import UX qualification before the milestone can close. Existing pre-v2 records with an empty hash are not silently walked from the UI thread; re-import or an explicit bounded background backfill is required before they can participate in duplicate review.
- M1 still requires real Windows 11 clean-machine/manual HiDPI/import/device-switching checks and physical master/cue/booth verification.
- Representative music-domain BPM/key/grid evidence, production key-lock listening/latency, controller profiles, long-session soak and physical recording/microphone evidence remain separate gates.

## Next largest step

Get this exact PR head green on Linux and Windows first. Then finish M4 with bounded background hash backfill for legacy rows plus real playback-history wiring/review, rather than widening optional DSP scope.
