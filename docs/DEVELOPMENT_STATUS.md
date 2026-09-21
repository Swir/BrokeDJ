# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `f8084c9e51b79c76676f0e20b81a14598d647893`. PR #65 (`M4: qualify large-library history and backfill bounds`) was integrated after exact-head workflow `35570973881` completed successfully; post-merge main workflow `35572013939` also completed successfully.
- Active development: PR #66, branch `feat/m4-native-library-scale-smoke`. Functional package head before this checkpoint documentation commit: `75d68a862445830bca6506a0eb894f0a266db397`.
- Exact-head workflows started for that functional head: normal build/test run `35572867859` and focused native large-library run `35572867933`. This documentation commit changes the PR head, so both workflows must pass again for the final head before merge.
- The package adds a Windows-native, no-audio lifecycle qualification against a deterministic 5,000-track / 12,000-history local SQLite fixture for both the development EXE and a freshly staged EXE.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). M4 is materially advancing but is not complete.
- GitHub Releases remains empty; no public BrokeDJ release is qualified by this checkpoint.

## Integrated M4 foundations on main

- Local SQLite schema/migrations, bounded search, native tag editing, ordered playlist membership editing, play-history storage, duplicate grouping, missing/moved-file rebinding, integrity checking and backup/restore are integrated without modifying original music files.
- Readable local imports receive streaming SHA-256 content identity outside the realtime callback; relocation refreshes that identity, while `is:duplicate` and `is:missing` remain non-destructive review filters.
- Real playback-start edges are recorded on a background worker into the local SQLite history, and a native PL/EN `HISTORY` window reads a bounded recent-history view without database work in the audio callback.
- Legacy non-missing rows with empty hashes receive a cancellable background backfill limited to 64 candidates per application start; disconnected paths are marked missing without deleting or overwriting source audio.
- Large-library automated coverage now seeds the production schema with 5,000 tracks and 12,000 history rows, requires the 250-row history window to stay bounded, verifies two independent 64-candidate legacy maintenance passes, normalizes Windows/Linux missing-file classification and rejects unstable file snapshots before publishing SHA-256 identity.
- Four-deck plus mixer session snapshots are bounded/versioned/checksummed, support verified backup recovery, and keep session I/O outside the realtime callback.
- Native Save Session / Load Session / verified `.bak` recovery pauses transports, uses the normal asynchronous decoder, reconnects moved library records when possible and does not auto-resume saved playback.
- Empty saved session slots use the realtime-safe deck-eject mailbox: the audio callback never deletes the retired clip and deterministic realtime tests require zero heap allocations/deallocations during clear adoption.
- Persistent waveform previews cache only bounded normalized peak data with source size/mtime validation. Cache I/O stays in decoder/background work and stale/corrupt entries fail closed to regeneration.

## Active M4 native large-library qualification package

- `scripts/library_scale_smoke.py` first runs the real BrokeDJ Windows EXE with `--smoke-test` and no audio device, then verifies that the app-created database is the production migrated schema before seeding any fixture rows.
- The harness refuses a non-empty database and refuses destructive local use outside GitHub Actions unless `--allow-local` is explicitly supplied. All fixture paths are synthetic and deliberately nonexistent; it does not create, overwrite or inspect user music files.
- It seeds exactly 5,000 synthetic track rows and 12,000 playback-history rows, reruns the native resize/lifecycle smoke, then requires unchanged fixture cardinality plus `PRAGMA quick_check=ok`.
- A focused Windows workflow runs the same qualification against the normal development EXE and then against a freshly staged EXE, retaining JSON evidence as a CI artifact.
- The native smoke requires `plays_audio=false`, `opens_audio_device=false`, four deterministic resize/lifecycle steps and valid content bounds. Elapsed startup/lifecycle timings are diagnostic only and are not performance, latency or hardware guarantees.
- The existing full build workflow remains unchanged and still gates Linux ASan/UBSan/progress/package checks plus Windows x64 build/full CTest/audio diagnostics/no-audio GUI/device/package smoke.

## Gates still open

- PR #66 remains development-only until both the normal exact-final-head GitHub Actions workflow and the focused native large-library workflow are green for the same final head.
- Automated native startup/resize evidence with a populated local database does not replace a user-controlled Windows 11 large-library import/search/tag/playlist interaction witness. M4 remains open until that broader workflow and recovery UX are reviewed.
- Automated build/tests do not replace Windows 11 clean-machine/manual HiDPI review, physical audio-device switching, master/cue/booth isolation, reviewed listening, controller input or live reliability qualification.
- Representative music-domain BPM/key/grid evidence, production key-lock listening/latency, controller profiles, long-session soak and physical recording/microphone evidence remain separate gates.

## Next largest step

Require green exact-final-head PR #66 runs and fix any regression before integration. Then continue M4 FINISH FIRST with a repeatable user-controlled Windows 11 library/import/search/tag/playlist witness and recovery review rather than widening optional DSP scope.
