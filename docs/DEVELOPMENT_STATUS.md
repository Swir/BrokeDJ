# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `3e3ee9b96e18ccc62de43b20c4976bf13aa4116c`. PR #64 (`M4: wire playback history and bounded legacy hash backfill`) is integrated after exact-head workflow `35568715694` completed successfully; its post-merge main workflow `35569629015` was still in progress when this checkpoint was written.
- Active development: PR #65, branch `feat/m4-library-scale-hardening`. Functional package head before this checkpoint documentation commit: `70312bfc692a9377b8f4780b571929fd8d4d32e3`; workflow `35569900558` had been assigned and was pending. The documentation commit changes the PR head, so a fresh exact-head run is required before merge.
- This package qualifies bounded library-history/backfill behavior against a 5,000-track / 12,000-history synthetic production-schema fixture and hardens legacy hash publication against filesystem inspection errors or files that change while hashing.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). M4 is materially advancing but is not complete.
- GitHub Releases remains empty; no public BrokeDJ release is qualified by this checkpoint.

## Integrated M4 foundations on main

- Local SQLite schema/migrations, bounded search, native tag editing, ordered playlist membership editing, play-history storage, duplicate grouping, missing/moved-file rebinding, integrity checking and backup/restore are integrated without modifying original music files.
- Readable local imports receive streaming SHA-256 content identity outside the realtime callback; relocation refreshes that identity, while `is:duplicate` and `is:missing` remain non-destructive review filters.
- Real playback-start edges are recorded on a background worker into the local SQLite history, and a native PL/EN `HISTORY` window reads a bounded recent-history view without database work in the audio callback.
- Legacy non-missing rows with empty hashes receive a cancellable background backfill limited to 64 candidates per application start; disconnected paths are marked missing without deleting or overwriting source audio.
- Four-deck plus mixer session snapshots are bounded/versioned/checksummed, support verified backup recovery, and keep session I/O outside the realtime callback.
- The native PL/EN local-library workflow provides bounded type-ahead search, multi-file import, explicit target-deck loading and moved-file relocation on background workers.
- Native Save Session / Load Session / verified `.bak` recovery pauses transports, uses the normal asynchronous decoder, reconnects moved library records when possible and does not auto-resume saved playback.
- Empty saved session slots use the realtime-safe deck-eject mailbox: the audio callback never deletes the retired clip and deterministic realtime tests require zero heap allocations/deallocations during clear adoption.
- Persistent waveform previews cache only bounded normalized peak data with source size/mtime validation. Cache I/O stays in decoder/background work and stale/corrupt entries fail closed to regeneration.

## Active M4 large-library hardening package

- The runtime-store test now seeds the real migrated library schema with 5,000 track rows and 12,000 playback-history rows, then requires the native recent-history query to return exactly its requested 250-row newest-first window.
- The same fixture keeps 1,024 legacy empty-hash candidates pending and executes two successive 64-row maintenance passes. Each pass must inspect and mark only its explicit bound, proving maintenance resumes from later candidates rather than walking the whole library in one startup pass.
- Diagnostic-only microsecond timings for the bounded history query and the two maintenance passes are printed for CI evidence; shared-runner timing is not a performance or latency certification.
- Legacy hash maintenance now distinguishes a filesystem inspection failure from a confirmed non-regular/missing source. An inspection error increments the failure diagnostic rather than incorrectly marking the track missing.
- Before a computed SHA-256 is published, both file size and `last_write_time` must match the pre-hash snapshot. A source that changes while hashing is skipped for a later pass, avoiding publication of identity from an unstable file snapshot.
- All filesystem/hash/SQLite work remains outside the audio callback, and the package does not modify original music files.

## Gates still open

- PR #65 remains development-only until its exact-final-head GitHub Actions run is green across Linux sanitizer/progress checks and Windows x64 build/full CTest/audio diagnostics/native no-audio GUI/device/package smoke.
- Automated synthetic scale evidence does not replace a user-controlled interactive large-library/import UX witness on Windows 11. M4 remains open until that broader workflow is reviewed along with the already-integrated persistence/recovery features.
- Automated build/tests do not replace physical Windows 11 audio-device qualification, reviewed listening, controller input or master/cue/booth verification.
- M1 still requires real Windows 11 clean-machine/manual HiDPI/import/device-switching checks and physical master/cue/booth verification.
- Representative music-domain BPM/key/grid evidence, production key-lock listening/latency, controller profiles, long-session soak and physical recording/microphone evidence remain separate gates.

## Next largest step

Require a green exact-final-head PR #65 run and fix any regression before integration. Then continue M4 FINISH FIRST with a repeatable interactive large-library/import witness and UX/recovery review rather than widening optional DSP scope.
