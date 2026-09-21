# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `bd04efadb412bbc506a09b88a541bebeb3275f2d`. PR #66 (`M4: add native large-library lifecycle qualification`) was integrated only after exact-head build/test run `35573083933` and focused native-library run `35573083971` completed successfully. Post-merge main runs `35576169961` (build/test) and `35576169949` (native library scale smoke) also completed successfully.
- Active development: PR #67, branch `feat/m4-playlist-browse-filter`. Latest functional head before this checkpoint documentation commit: `60d55303306119312a599d360cb902cd470f6d8e`.
- Self-review blocker fixed on that functional head: playlist-catalog refresh now uses a dedicated background `ThreadPool`, independent from the cancellable playlist-browse queue, so an immediate post-edit search cannot cancel a queued catalog-count refresh. The catalog worker is generation-checked and drained during shutdown.
- Exact-head workflows started for that functional head: normal build/test run `35578860090` and focused native large-library run `35578860068`. This checkpoint commit changes the PR head, so required checks must pass again for the final head before merge.
- PR #67 adds a bounded native playlist selector to the local library, playlist-scoped title/artist/album/path/tag search, `is:duplicate` / `is:missing` review inside a playlist, selected-track playlist membership visibility and refreshed playlist counts after membership edits.
- The focused large-library harness seeds deterministic playlist/tag relationships in addition to 5,000 tracks and 12,000 history rows and verifies the same 500-row playlist-query bound and review directives used by the native UI contract.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). M4 is materially advancing but is not complete.
- GitHub Releases remains empty; no public BrokeDJ release is qualified by this checkpoint.

## Integrated M4 foundations on main

- Local SQLite schema/migrations, bounded search, native tag editing, ordered playlist membership editing, play-history storage, duplicate grouping, missing/moved-file rebinding, integrity checking and backup/restore are integrated without modifying original music files.
- Readable local imports receive streaming SHA-256 content identity outside the realtime callback; relocation refreshes that identity, while `is:duplicate` and `is:missing` remain non-destructive review filters.
- Real playback-start edges are recorded on a background worker into the local SQLite history, and a native PL/EN `HISTORY` window reads a bounded recent-history view without database work in the audio callback.
- Legacy non-missing rows with empty hashes receive a cancellable background backfill limited to 64 candidates per application start; disconnected paths are marked missing without deleting or overwriting source audio.
- Large-library automated coverage seeds the production schema with 5,000 tracks and 12,000 history rows, keeps recent-history reads bounded, verifies bounded legacy maintenance passes, normalizes Windows/Linux missing-file classification and rejects unstable file snapshots before publishing SHA-256 identity.
- PR #66 added a Windows-native no-audio lifecycle qualification against that deterministic production-schema fixture for both the development EXE and a freshly staged EXE, with `PRAGMA quick_check=ok` and unchanged fixture cardinality required after the native lifecycle.
- Four-deck plus mixer session snapshots are bounded/versioned/checksummed, support verified backup recovery, and keep session I/O outside the realtime callback.
- Native Save Session / Load Session / verified `.bak` recovery pauses transports, uses the normal asynchronous decoder, reconnects moved library records when possible and does not auto-resume saved playback.
- Empty saved session slots use the realtime-safe deck-eject mailbox: the audio callback never deletes the retired clip and deterministic realtime tests require zero heap allocations/deallocations during clear adoption.
- Persistent waveform previews cache only bounded normalized peak data with source size/mtime validation. Cache I/O stays in decoder/background work and stale/corrupt entries fail closed to regeneration.

## Active M4 playlist-browsing package

- The native PL/EN library panel exposes `All tracks` plus a bounded playlist catalog with per-playlist track counts. Playlist catalog reads use a separate read-only SQLite connection and remain outside the audio callback.
- Playlist catalog refresh and playlist browsing use independent worker queues. Search cancellation is therefore limited to stale browse jobs and cannot cancel post-edit catalog refreshes.
- Selecting a playlist executes bounded background reads (maximum 500 rows) in playlist order. Normal search covers title, artist, album, local path and tags; `is:duplicate` and `is:missing` keep their non-destructive review meaning inside the selected playlist.
- Selection metadata reports both tags and playlist memberships. Membership edits refresh the selected-track metadata and playlist catalog rather than mutating source music.
- The native large-library fixture adds eight playlists, 5,000 playlist-item rows, 32 tags and 5,000 track-tag rows. Deterministic checks cover the 500-row bound, title search, tag search, duplicate review and missing-file review before and after the native no-audio lifecycle.
- The focused Windows workflow applies the fixture to the real development EXE and then to a freshly staged EXE. It never creates or edits music files and does not open an audio device.

## Gates still open

- PR #67 remains development-only until required exact-final-head GitHub Actions checks are green for the same final head. A green earlier functional head is not sufficient after checkpoint/documentation changes.
- Automated native startup/resize and SQL-contract evidence with a populated local database does not replace a user-controlled Windows 11 library import/search/tag/playlist interaction witness. M4 remains open until that broader workflow and recovery UX are reviewed.
- Automated build/tests do not replace Windows 11 clean-machine/manual HiDPI review, physical audio-device switching, master/cue/booth isolation, reviewed listening, controller input or live reliability qualification.
- Representative music-domain BPM/key/grid evidence, production key-lock listening/latency, controller profiles, long-session soak and physical recording/microphone evidence remain separate gates.

## Next largest step

Require green exact-final-head PR #67 runs and fix any regression before integration. After this internally automatable playlist package is green, keep the user-controlled Windows M4 interaction/recovery witness explicit and finish any remaining internally closable M4 acceptance work before widening scope.
