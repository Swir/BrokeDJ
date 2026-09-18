# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`.
- PR #20 (`feat/deck-keylock-selector`) merged as `971d4ea418559792cab0f00aa95025ade5d841b5` after exact-final-head `f2c3b1336d1afd4fb22a5dbc79676b9fa157b43f` passed GitHub Actions run `35396076701`.
- That exact-head gate passed Linux ASan/UBSan with **17/17 CTest targets**, including the new selector functional and warmed realtime contracts, plus Windows x64 configure/build/full CTest/audio diagnostics/native no-audio GUI smoke/staging/artifact upload.
- The selector is still deliberately outside ordinary `Engine::process()` and exposes no GUI control. Production playback remains the default and immediate fallback.
- Roadmap counter remains **M0 complete; 1/10 equal-weight milestones = 10.0%**.

## Verified deck-owned key-lock selector boundary

PR #20 adds `DeckPlaybackSelector` above the already-qualified `TimeStretchEngineBridge`. It is the first deck-level ownership slice for eventual production key lock without changing ordinary playback.

The selector now:

- owns transactional off-callback prepare/stage/disarm lifecycle for one deck-level key-lock path;
- retains the existing production-style pitch-changing converter as immediate fail-closed fallback;
- publishes transport and algorithm-latency-compensated audible cursors separately for future deck meter/sync ownership;
- adds a prepared 5 ms path-transition de-click when switching between fallback and stretch output;
- never re-primes, allocates, decodes, performs I/O, logs or locks inside the measured render path;
- fails closed on unstaged seek/cursor discontinuity, clip replacement and invalid staged controls, and requires explicit off-callback restaging before stretch can resume;
- supports exact loop identity and explicit disarm without allowing stale prefetched stretch audio to resume;
- exposes bounded path/fallback diagnostics for later Engine integration.

## Validation state

- PR #20 exact-final-head run `35396076701` is green across Linux and Windows development gates.
- Linux completed 17/17 CTest targets under ASan/UBSan; the selector realtime target rendered 500 warmed blocks and retained the zero-heap allocation/deallocation contract.
- Windows x64 completed configure/build, full CTest, audio diagnostic replay, native no-audio GUI lifecycle smoke, staging and artifact upload.
- Shared-runner timing remains diagnostic only. No physical Windows 11 audio interface, controller, reviewed music-domain listening, measured device latency or hardware-underrun qualification is claimed.
- `docs/progress.json` remains unchanged at 1/10 = 10.0%; this selector qualification does not close M1 or the broader M2 performance-deck milestone.

## Remaining blockers / gates

1. Production `Engine::process()` still uses the current pitch-changing hybrid converter; the qualified selector must be connected behind an opt-in Engine/deck ownership boundary before any user-visible key-lock control exists.
2. The first Engine hookup must preserve the existing deck mix/EQ/FX/cue semantics, publish the correct audible-vs-transport meter position and keep fallback immediate under load/seek/loop/rate/pitch changes.
3. The selector transition is a deterministic de-click gate, not proof of transparent perceptual switching; reviewed music-domain listening remains open.
4. Algorithm-latency compensation is scheduling metadata, not physical device latency.
5. Clean Windows 11 interactive launch, resize/import and physical two-/four-output audio-interface behavior remain manual M1 gates.
6. Beat/tempo/key analysis, editable beat grids, hotcues, beat loops, slip/reverse/scratch and the remainder of M2 remain open.

## Next highest-impact step

Wire the qualified selector into the smallest opt-in production Engine/deck boundary while leaving ordinary playback as the default. Keep lifecycle preparation off callback, preserve per-deck gain/EQ/FX/cue/meter behavior, add deterministic integration fixtures for load/seek/loop/rate/pitch/fallback transitions, and still expose no GUI key-lock control until that exact production path is qualified.
