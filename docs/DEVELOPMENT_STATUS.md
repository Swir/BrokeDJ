# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`; active integration branch: `feat/performance-deck-owner`; open PR: #31 (`M2: expand reviewed performance-deck owner contract`).
- Current validated implementation head before this checkpoint document: `1b1468076bd777e5c78b62cd7da4c1c97cb3609c`.
- Exact-head GitHub Actions run `35429010447` is green: Linux core/sanitizer validation passed, and Windows x64 completed configure/build/full core CTest/audio diagnostics/native no-audio GUI lifecycle smoke/staging/artifact upload.
- PR #31 remains deliberately unmerged. A green CI development artifact is not a public release or hardware/live-readiness qualification.
- Roadmap counter remains **M0 complete; 1/10 equal-weight milestones = 10.0%**. The current work advances M2 but does not close M1 or M2.

## M2 performance-deck work in PR #31

The branch now has one JUCE-independent `PerformanceDeckOwner` per deck on top of the reviewed `BeatGrid` and production `Engine` transport boundary.

Implemented and validated at the code-head snapshot:

- whole-track LOOP and reviewed beat-length LOOP remain separate user modes;
- reviewed beat-loop planning is transactional and rejects invalid/out-of-track regions before control state changes;
- the native deck UI exposes explicit **1 / 2 / 4 / 8 / 16 beat** loop choices only after a valid reviewed grid is available;
- selected automatic/manual grids are republished to the owner after analysis or editing, while clip replacement/grid invalidation clears stale source-bound performance state;
- eight hotcue slots exist at the core owner boundary with optional reviewed-grid quantization; hotcue UI/session persistence is not exposed yet;
- reviewed-grid beat jump preserves fractional beat phase and fails closed when the destination is outside the track;
- bounded one-shot follower sync validates both reviewed grids, deck ownership, rate limits and phase-correction limits before mutating follower rate/seek;
- self-sync and cross-Engine sync are rejected, and the implementation is explicitly not continuous phase lock;
- active beat-derived loops are invalidated conservatively before hotcue, beat-jump or sync transport moves when required;
- the existing optional key-lock source-renderer ownership remains respected; conflicting beat-loop requests fail closed rather than stealing the deck source path;
- no new decode, disk/network I/O, plugin work, blocking lock or allocation was added to the realtime callback.

## Deterministic coverage

`PerformanceDeckOwnerTests` covers reviewed variable-tempo loop ownership, failed tail re-arm, grid replacement, whole-track/beat-loop separation, quantized/unquantized hotcues, beat-jump phase preservation and bounds, one-shot 120→128 BPM sync application, phase-safety rejection without control drift, self-/cross-Engine sync rejection, renderer conflicts, invalid decks and clip-reset invalidation.

The existing Engine/realtime suites continue to cover the production custom loop-region renderer, de-click behavior, source replacement safety and zero-heap callback contract. The Windows job additionally runs the repository audio diagnostics and native GUI lifecycle smoke without audio hardware.

## Validation state

- PR #28 merged as `f917fabc579723044be706e3eb86971cb48a67ad` after run `35421304445`.
- PR #29 merged as `473573962047d31b188915c3e47df79654baecf6` after run `35421909580`.
- PR #30 merged as `e7d67e96aa514c9f229daba92e3fca849acb0522` after run `35423792947`.
- PR #31 implementation head `1b1468076bd777e5c78b62cd7da4c1c97cb3609c` passed run `35429010447` on Linux and Windows, including Windows staging/artifact upload.
- No physical Windows 11 audio interface, real four-output cue, controller, reviewed music-domain listening, measured device latency, hardware-underrun qualification or representative copyrighted-music corpus is claimed.
- `docs/progress.json` remains unchanged at **1/10 = 10.0%**.

## Remaining blockers / gates

1. Physical Windows 11 clean-machine launch, real audio-device switching and physical two-/four-output cue still block M1 completion.
2. The analysis validator still needs a legally usable/local representative music corpus and recorded results before stronger BPM/key claims.
3. Hotcues remain an in-memory owner contract; normal pads, persistence/session migration, controller mapping and transport regression are open.
4. Beat jump and one-shot sync are core owner contracts only; normal UX/master selection and additional render/listening qualification are open.
5. The developer key-lock lifecycle still requires reviewed music-domain listening and stronger live transition/race/device qualification before normal GUI exposure.
6. Slip, reverse, scratch and broader deck workflows remain open.

## Next highest-impact step

Keep PR #31 as the active M2 integration slice. Next, add normal eight-pad hotcue UX plus durable source/session persistence with migration and corruption handling, then expose beat jump and a conservative master/follower sync workflow using the already bounded owner contracts. Continue to keep physical M1 audio-interface checks as a separate manual gate and do not infer them from CI.
