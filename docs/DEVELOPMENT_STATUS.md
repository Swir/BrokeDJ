# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `3d0b2c0b124c83e6fa995a852a0e4900b9086a2b`; PR #54 (`M4: add local library database foundation`) is merged after exact-head run `35538728696` succeeded.
- Active development: PR #56 on `feat/m4-native-library-workflow`.
- Native-workflow implementation checkpoint before this status commit: `3d23631941060daaa661d8f07aaacb02384e5d22`. The status commit itself is part of the final head and requires its own Linux/Windows/package CI before merge.
- The branch changes only the app/UI layer: the audio core remains unchanged and library database/file work is isolated from `MainComponent::getNextAudioBlock()`.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). This M4 work does not close M4.
- GitHub Releases remains empty; no public BrokeDJ Release is qualified by this checkpoint.

## Active M4 slice: native local library workflow

- The merged SQLite foundation now has a native PL/EN library panel with bounded search, multi-file import, explicit target-deck loading and moved-file relocation.
- Type-ahead search uses a dedicated single worker with queued work collapsed to the latest query; imports/writes use a separate single worker with bounded batch size and shutdown cancellation.
- Missing files fail closed and can be reconnected without changing stable track IDs or relationships. Direct deck loading remains available if the local database cannot be opened.
- Local library paths stay local. This slice adds no cloud/account dependency and does not log private music paths into public diagnostics.

## Gates still open

- PR #56 must not merge until its exact-final-head Linux/Windows/package workflow is fully green, including native no-audio GUI construction/resize smoke.
- M4 still needs playlist/tag editing UI, content-hash indexing, analysis/waveform-cache ownership, complete session persistence, backup/restore UI and real large-library qualification.
- M1 still requires real Windows 11 clean-machine/manual HiDPI/import/device-switching checks and physical master/cue/booth verification.
- Representative music-domain BPM/key/grid evidence, production key-lock listening/latency, controller profiles, long-session soak and physical recording/microphone evidence remain separate gates.

## Next largest step

Repair any exact-head regression in PR #56 before integration. After the native library workflow is green, continue FINISH FIRST inside M4 with real playlist/tag management and session persistence rather than widening optional DSP scope.
