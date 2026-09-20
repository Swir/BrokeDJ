# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `dfbd62a43a5cab3fcae4d6dcc7c01ff58365ba3b`; PR #53 (`M3: qualify three-band EQ response`) is merged after exact-head run `35534970058` succeeded.
- Active development: PR #54 on `feat/m4-library-db-foundation`.
- Implementation checkpoint before this status-only commit: `09151c15f4a06efabf7ff97bfaef066dcb721e79`; PR workflow run `35538706728` was queued when recorded. The status update itself requires its own exact-head CI before merge.
- Local verification: core-only ASan/UBSan build passed 12/12 CTest targets; the new library tests passed with strict GCC warnings and again under ASan/UBSan using the available local SQLite 3.46.1. Windows/MSVC plus the pinned SQLite 3.53.4 path remain CI-gated.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). This M4 foundation does not close M4.
- GitHub Releases remains empty; no public BrokeDJ Release is qualified by this checkpoint.

## Active M4 slice: local library database foundation

- Added a JUCE-independent application-side SQLite adapter for track metadata, search, tags, ordered playlists and play history.
- Added missing/moved-file rebinding and content-hash duplicate grouping without touching original music files.
- Added transactional schema migration, future-schema rejection, bounded backup retries and integrity-checked backup/restore; corrupt/future backups are rejected before replacement.
- SQLite 3.53.4 is pinned to the official amalgamation SHA3-256 in CMake and documented in `THIRD_PARTY_NOTICES.md`; database/file I/O stays outside the realtime audio callback.

## Gates still open

- PR #54 must not merge until its exact-final-head Linux/Windows/package workflow is green.
- M4 still needs native library/import UI integration, analysis/waveform-cache ownership, complete session persistence and real large-library/backup workflow qualification.
- M1 still requires real Windows 11 clean-machine/manual resize/HiDPI/import/device-switching checks and physical master/cue/booth verification.
- Representative music-domain BPM/key/grid evidence, production key-lock listening/latency, controller profiles, long-session soak and physical recording/microphone evidence remain separate gates.

## Next largest step

First repair any PR #54 build/test regression exposed by exact-head CI. Once the database foundation is green, continue FINISH FIRST by connecting the native import/search/library workflow to this persistence layer before widening optional DSP scope.
