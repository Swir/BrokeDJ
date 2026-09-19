# BrokeDJ development status

This file is the durable engineering checkpoint for work that is not yet merged to `main`. It is not a release-readiness or live-performance claim.

## Current checkpoint

- Default branch: `main`.
- Verified `main` baseline: `ebe5866a31673c7b5861ccead2c9b5819202526e` (merged PR #31).
- Baseline CI: run `35431296587` — successful Linux sanitizer/core and Windows x64 development-build gates.
- Active branch: `feat/hotcue-persistence`.
- Pull request: #32, `M2: add source-bound hotcue persistence store` — open, not merged.
- Verified implementation head: `9de066960975907229bd07fa97d8a25a5e29dc60`.
- Exact implementation CI: run `35433441986` — successful on both jobs.
- Roadmap source of truth remains `docs/progress.json`: 1/10 milestones complete (10.0%, PRE-ALPHA).
- No public BrokeDJ Release is qualified by this checkpoint.

## Implemented in PR #32

1. **Source-bound persistent Hot Cue state**
   - Versioned eight-slot snapshot format backed by the exact `PerformanceDeckOwner::HotCue` model.
   - State is bound to source size and modification time; changed files reject stale cue state.
   - The local source path is used only to derive the state-file key. The payload does not contain the raw path or filename.
   - Payloads are bounded to 16 KiB and reject invalid, unknown-schema, corrupt and oversized records.
   - Writes use `juce::TemporaryFile` replacement so an interrupted replacement does not intentionally overwrite the last valid state with a partial payload.

2. **Transactional owner restoration**
   - `PerformanceDeckOwner` can export and restore a complete `HotCueBank` without re-quantizing persisted source-time positions.
   - The whole bank is validated before mutation. Invalid or out-of-track persisted data leaves the previous complete in-memory bank unchanged.
   - The app-side store exposes `storeFrom(...)` and `loadInto(...)` adapters so future native lifecycle wiring does not duplicate persistence validation.
   - No disk access was added to `Engine::process()` or any audio callback path.

3. **Deterministic persistence/integration coverage**
   - Round-trip metadata and unset-slot preservation.
   - Payload path/filename privacy checks.
   - Source-identity invalidation.
   - Atomic replacement behavior and invalid-write non-clobbering.
   - Unknown-schema, oversized and corrupt-record fail-closed behavior.
   - Owner -> persistent store -> restored owner -> transport-trigger round trip.
   - Failed complete-bank restore preserves the previous valid bank.

## Verification

Exact implementation head `9de066960975907229bd07fa97d8a25a5e29dc60` passed GitHub Actions run `35433441986`:

- Linux `ubuntu-24.04`: generated-progress check, sanitizer/core build and CTest — success.
- Windows `windows-2022`: native configure/build, full CTest (including `performance_state_store`), audio render/callback diagnostics, no-audio native GUI lifecycle smoke, development staging and artifact upload — success.

The Windows CI smoke does not replace physical audio-interface, multi-output cue, controller, clean-machine listening or soak qualification.

## Gates still open

- M1 physical Windows 11 audio-device qualification, including verified four-output cue routing where the interface supports it.
- Representative legal local-music corpus validation for BPM/key/grid behavior.
- Normal Hot Cue 1–8 pads and lifecycle wiring in the native app; persistence exists but the regular user-facing pads are not exposed by PR #32.
- Moved-file recovery, session-level migration policy and controller mappings.
- Real listening/soak testing and production release qualification.

## Next largest step

Wire `TrackHotCueStore` into the native deck load/edit/clear lifecycle, expose compact Hot Cue 1–8 controls without crowding the deck layout, and add native GUI/lifecycle tests for load -> restore -> set/clear -> reload. After that, expose Beat Jump and bounded master/follower Sync controls on reviewed grids while keeping all analysis and persistence work off the audio callback.
