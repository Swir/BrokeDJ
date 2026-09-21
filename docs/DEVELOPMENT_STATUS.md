# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `53653d37a64fb3fba3843073f473c371b6bd1155`. PR #60 (`M4: restore empty session slots with realtime-safe deck eject`) passed exact-head workflow `35550620903` and was squash-merged after that green gate.
- Active development branch: `feat/m4-library-organization-ui`. The current package adds native local tag editing and playlist membership editing to the library panel; its exact final head must pass Linux sanitizer/CTest and Windows x64 build/full CTest/no-audio GUI/device/package checks before any merge.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). M4 is materially advancing but is not complete.
- GitHub Releases remains empty; no public BrokeDJ release is qualified by this checkpoint.

## Integrated M4 foundations on main

- Local SQLite schema/migrations, bounded search, tags, ordered playlists, play history, duplicate grouping, missing/moved-file rebinding, integrity checking and backup/restore are integrated without modifying original music files.
- Four-deck plus mixer session snapshots are bounded/versioned/checksummed, support verified backup recovery, and keep session I/O outside the realtime callback.
- The native PL/EN local-library workflow provides bounded type-ahead search, multi-file import, explicit target-deck loading and moved-file relocation on background workers.
- Native Save Session / Load Session / verified `.bak` recovery pauses transports, uses the normal asynchronous decoder, reconnects moved library records when possible and does not auto-resume saved playback.
- Empty saved session slots now use the realtime-safe deck-eject mailbox: the audio callback never deletes the retired clip and deterministic realtime tests require zero heap allocations/deallocations during clear adoption.

## Active M4 library-organization package

- The local library panel now exposes PL/EN tag add/remove controls for the selected track using the existing SQLite tag model.
- The panel exposes playlist `add/create` and `remove from` membership controls. Removal resolves only an already-existing playlist and does not create a typo/placeholder playlist.
- Tag metadata for the selected row is refreshed asynchronously; stale selection responses are discarded through a generation counter.
- Editing uses bounded single-worker queues and a separate SQLite `FULLMUTEX` connection. No database operation, file I/O or UI edit is added to `Engine::process()`.
- The existing library search is re-run after successful tag edits so tag-based queries reflect the persisted change. Original music files are never edited or deleted by these controls.

## Gates still open

- The active branch is development-only until exact-final-head CI is green; a successful compile alone is not sufficient.
- M4 still needs stronger playlist management UX, content-hash population/indexing from imported sources, analysis/waveform-cache ownership and large-library qualification before the milestone can close.
- M1 still requires real Windows 11 clean-machine/manual HiDPI/import/device-switching checks and physical master/cue/booth verification.
- Representative music-domain BPM/key/grid evidence, production key-lock listening/latency, controller profiles, long-session soak and physical recording/microphone evidence remain separate gates.

## Next largest step

Qualify the native tag/playlist package on the exact final head and fix any regression first. Then continue M4 FINISH FIRST with content-hash population/duplicate handling and analysis/waveform-cache ownership rather than widening optional DSP scope.
