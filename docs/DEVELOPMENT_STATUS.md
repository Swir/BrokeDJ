# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `3d0b2c0b124c83e6fa995a852a0e4900b9086a2b`; PR #54 (`M4: add local library database foundation`) is merged.
- Active development: `feat/m4-session-persistence`; the implementation checkpoint before this status commit is `2ca4323291374073ffa2c41b96ea774661f68317`.
- Local verification: the new header-only session store passed a strict standalone C++20 compile with GCC (`-Wall -Wextra -Wpedantic -Wconversion -Werror`). Full repository Linux/Windows CI is required on the exact PR head before integration.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). This M4 slice does not close M4.
- GitHub Releases remains empty; no public BrokeDJ Release is qualified by this checkpoint.

## Active M4 slice: session persistence foundation

- Added a bounded local session snapshot for four deck source paths, transport positions, playback rates, trim/fader/EQ/FX controls, cue/loop/play-state markers and master/crossfader/headphone mixer state.
- Added schema/version checks, bounded file/path sizes, non-finite/range rejection and a checksum so corrupted, truncated or newer-format session files fail closed.
- Saving uses a verified temporary file plus previous-file backup/rollback handling so an invalid replacement does not intentionally destroy the last valid session.
- Deterministic tests cover mixer/deck round-trip, four-deck independence, rejected NaN state, future-schema rejection, checksum corruption and oversized paths. Session I/O remains outside the realtime callback and never modifies source music.

## Gates still open

- This branch must pass exact-final-head Linux/Windows/package CI before any merge.
- M4 still needs native library/import/search UI integration, app-side Save/Load Session wiring, missing/moved-track restore behavior, analysis/waveform-cache ownership and user-visible backup/restore qualification.
- M1 still requires real Windows 11 clean-machine/manual resize/HiDPI/import/device-switching checks and physical master/cue/booth verification.
- Representative music-domain BPM/key/grid evidence, production key-lock listening/latency, controller profiles, long-session soak and physical recording/microphone evidence remain separate gates.

## Next largest step

First repair any exact-head regression in this session package. Then continue FINISH FIRST by connecting the existing native import/search/library workflow to `LibraryDatabase` and wiring explicit Save Session / Load Session commands before widening optional DSP scope.
