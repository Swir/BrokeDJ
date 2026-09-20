# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `cca46094fdb291446d5d77c8ec2271b04c35c6a6`; PR #55 (`M4: add bounded session persistence foundation`) is merged after exact-final-head workflow `35539900396` succeeded on Linux ASan/UBSan, Windows x64 build/tests/diagnostics/GUI smoke/device probe, staging and downloaded-artifact smoke.
- Active development: PR #56 on `feat/m4-native-library-workflow`. Native-workflow implementation plus current-main merge parent: `40227ae4ff5b11927067caa1731039f51f194351`; that merge has parents `07c9f45f9f4f943bd61adf7051d14b91149b8bff` (native workflow) and `cca46094fdb291446d5d77c8ec2271b04c35c6a6` (current main), with no force-push and no parallel session changes overwritten.
- This status commit is intentionally the final branch checkpoint after that merge and requires its own exact-head Linux/Windows/package workflow before PR #56 can merge.
- The pre-sync native workflow run `35539872383` already passed Linux sanitizers plus Windows build/tests/audio diagnostics/native no-audio GUI smoke/device probe; it is useful regression evidence but is not merge evidence for the new integrated head.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). These M4 slices do not close M4.
- GitHub Releases remains empty; no public BrokeDJ Release is qualified by this checkpoint.

## Integrated M4 foundations

- The local SQLite library foundation provides schema migrations, bounded search, tags, ordered playlists, play history, duplicate grouping, missing/moved-file rebinding, integrity checks and backup/restore without modifying original music.
- The merged session foundation provides bounded/versioned four-deck plus mixer snapshots, checksum/range validation, verified temporary publication, rollback handling and explicit verified backup recovery. Session I/O remains outside the realtime callback.
- PR #56 adds a native PL/EN local-library panel with bounded type-ahead search, multi-file import, target-deck loading and moved-file relocation. Search and writes use separate bounded single-worker paths and shutdown cancellation; no cloud/account dependency is added.
- Missing files fail closed and can be reconnected without changing stable track IDs. Direct deck loading remains available if the local library database cannot be opened.

## Gates still open

- PR #56 must pass exact-final-head Linux/Windows/package CI after this main sync before any merge.
- M4 still needs playlist/tag editing UI, content-hash indexing, analysis/waveform-cache ownership, app-side Save/Load Session commands, missing/moved-track session restore behavior, user-visible backup/restore and real large-library qualification.
- M1 still requires real Windows 11 clean-machine/manual HiDPI/import/device-switching checks and physical master/cue/booth verification.
- Representative music-domain BPM/key/grid evidence, production key-lock listening/latency, controller profiles, long-session soak and physical recording/microphone evidence remain separate gates.

## Next largest step

Repair any exact-head regression in PR #56 before integration. Once the native library workflow is green on top of the merged session foundation, continue FINISH FIRST by wiring explicit Save Session / Load Session commands and restore-safe deck loading before widening optional DSP scope.
