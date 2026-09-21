# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `9343d9cd88034d3d9c94167ba9728936840d5c3f`. PR #61 (`M4: add native tag and playlist membership editing`) passed exact-head workflow `35553528800` and was merged only after Linux sanitizer/CTest and Windows x64 build/full CTest/no-audio GUI/device/package checks were green.
- Active development branch: `feat/m4-waveform-cache`. The current package adds a persistent validated 512-point waveform-preview cache to the existing decoder worker path. Its exact final head must pass Linux sanitizer/CTest and Windows x64 build/full CTest/no-audio GUI/device/package checks before merge.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). M4 is materially advancing but is not complete.
- GitHub Releases remains empty; no public BrokeDJ release is qualified by this checkpoint.

## Integrated M4 foundations on main

- Local SQLite schema/migrations, bounded search, native tag editing, ordered playlist membership editing, play-history storage, duplicate grouping, missing/moved-file rebinding, integrity checking and backup/restore are integrated without modifying original music files.
- Four-deck plus mixer session snapshots are bounded/versioned/checksummed, support verified backup recovery, and keep session I/O outside the realtime callback.
- The native PL/EN local-library workflow provides bounded type-ahead search, multi-file import, explicit target-deck loading and moved-file relocation on background workers.
- Native Save Session / Load Session / verified `.bak` recovery pauses transports, uses the normal asynchronous decoder, reconnects moved library records when possible and does not auto-resume saved playback.
- Empty saved session slots use the realtime-safe deck-eject mailbox: the audio callback never deletes the retired clip and deterministic realtime tests require zero heap allocations/deallocations during clear adoption.
- Tag metadata refresh and tag/playlist writes use bounded worker queues and a separate SQLite `FULLMUTEX` connection. Search refreshes after successful tag edits so tag queries reflect persisted changes.

## Active M4 waveform-cache package

- `WaveformPreviewCache` persists only the 512 normalized amplitude peaks plus schema and source size/mtime validation fields. Raw local source paths and filenames are not stored in payloads; the path is used only to derive the cache-file key.
- Cache load/store runs inside decoder/background work, never `Engine::process()`. Cache failure is non-fatal: invalid, stale or corrupt records fail closed and the preview is rebuilt from the audio source.
- Both the bounded streaming preview path and the small-track in-memory decode path reuse a valid cached preview. Source identity changes invalidate the record automatically.
- Decoder diagnostics expose `waveformCacheHit` for deterministic validation without changing playback semantics. Tests use an isolated cache directory and cover first-build, hit, source-identity invalidation, corruption recovery and cache-disable behavior.

## Gates still open

- The active branch is development-only until exact-final-head CI is green; local/CI compilation and deterministic tests do not replace physical Windows audio-device qualification.
- M4 still needs content-hash population/indexing from imported sources, user-facing duplicate review/history closure and large-library qualification before the milestone can close.
- M1 still requires real Windows 11 clean-machine/manual HiDPI/import/device-switching checks and physical master/cue/booth verification.
- Representative music-domain BPM/key/grid evidence, production key-lock listening/latency, controller profiles, long-session soak and physical recording/microphone evidence remain separate gates.

## Next largest step

Qualify the waveform-cache package on its exact final head and fix any regression first. Then continue M4 FINISH FIRST with content-hash population and non-destructive duplicate review rather than widening optional DSP scope.
