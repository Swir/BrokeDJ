# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `0552cbb6d32aa00ba05720791e6188ae2df1584e`. PR #63 (`M4: populate library content identity and duplicate review`) is integrated; its post-merge main workflow `35565846275` completed successfully.
- Active development: PR #64, branch `feat/m4-runtime-history-backfill`. Functional package head before this checkpoint-only documentation commit: `ca4a7747cd6ce7c101ba519884ab38704bb7f2f7`. Exact-head workflow `35568563402` was in progress when this checkpoint text was written, so this package is not merged or release-qualified.
- This package wires real playback-start history outside the realtime callback, adds a native PL/EN history review window, and performs a cancellable bounded legacy content-hash backfill of at most 64 empty-hash rows per application start.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). M4 is materially advancing but is not complete.
- GitHub Releases remains empty; no public BrokeDJ release is qualified by this checkpoint.

## Integrated M4 foundations on main

- Local SQLite schema/migrations, bounded search, native tag editing, ordered playlist membership editing, play-history storage, duplicate grouping, missing/moved-file rebinding, integrity checking and backup/restore are integrated without modifying original music files.
- Readable local imports receive streaming SHA-256 content identity outside the realtime callback; relocation refreshes that identity, while `is:duplicate` and `is:missing` remain non-destructive review filters.
- Four-deck plus mixer session snapshots are bounded/versioned/checksummed, support verified backup recovery, and keep session I/O outside the realtime callback.
- The native PL/EN local-library workflow provides bounded type-ahead search, multi-file import, explicit target-deck loading and moved-file relocation on background workers.
- Native Save Session / Load Session / verified `.bak` recovery pauses transports, uses the normal asynchronous decoder, reconnects moved library records when possible and does not auto-resume saved playback.
- Empty saved session slots use the realtime-safe deck-eject mailbox: the audio callback never deletes the retired clip and deterministic realtime tests require zero heap allocations/deallocations during clear adoption.
- Persistent waveform previews cache only bounded normalized peak data with source size/mtime validation. Cache I/O stays in decoder/background work and stale/corrupt entries fail closed to regeneration.

## Active M4 runtime-history/backfill package

- The application observes deck playback-state edges on the message thread and queues SQLite history writes to a dedicated worker; no database or filesystem work is added to `Engine::process()` or the audio callback.
- Tracks played from outside the library are adopted into the local database on the history worker before the history event is written. Existing rows are resolved by exact normalized path without re-hashing the file on every play.
- A native `HISTORY` window displays the newest 250 local history rows with track metadata and timestamps, with PL/EN labels and bounded refresh work off the message thread.
- `LibraryRuntimeStore` scans at most 64 legacy non-missing rows with an empty content hash per launch. Files are hashed in 64 KiB chunks on a cancellable maintenance worker; disconnected paths are marked missing, and source music is never changed or deleted.
- Focused deterministic tests cover joined/repeated history ordering, exact path resolution, bounded SHA-256 backfill using the `abc` reference vector, missing-source handling, cancellation and a later resumed pass.

## Gates still open

- PR #64 remains development-only until exact-final-head GitHub Actions is green. The checkpoint documentation commit changes the PR head, so a fresh exact-head run is required even if the functional code head was already building.
- Automated build/tests do not replace physical Windows 11 audio-device qualification, reviewed listening, controller input or master/cue/booth verification.
- M4 still needs broader real-library/import UX qualification and an explicit large-library interactive pass before the milestone can close. The bounded backfill intentionally does not attempt an unbounded startup scan.
- M1 still requires real Windows 11 clean-machine/manual HiDPI/import/device-switching checks and physical master/cue/booth verification.
- Representative music-domain BPM/key/grid evidence, production key-lock listening/latency, controller profiles, long-session soak and physical recording/microphone evidence remain separate gates.

## Next largest step

Fix any PR #64 regression first and require a green exact-final-head Linux/Windows run. Keep the package on the development branch until that gate is satisfied; after integration, continue M4 FINISH FIRST with large-library/import UX qualification rather than widening optional DSP scope.
