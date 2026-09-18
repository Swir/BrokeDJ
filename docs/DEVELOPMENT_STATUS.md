# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`.
- PR #19 (`feat/engine-keylock-production-hook`) merged as `b06318332264423f6bb63c9b3be3d00d90e0dc5b` after exact-final-head `7b45e143234336d198d75f135af57628dcf6d379` passed GitHub Actions run `35390608930`.
- That exact-head gate passed Linux ASan/UBSan plus all 15 core/time-stretch CTest targets, and Windows x64 configure/build/full CTest/audio diagnostics/native no-audio GUI smoke/staging/artifact upload.
- PR #18 had already qualified the Engine-facing time-stretch boundary and fixed the real 96 kHz prepared-capacity regression. PR #19 builds on that boundary without changing ordinary `Engine::process()` playback.
- Roadmap counter remains **M0 complete; 1/10 equal-weight milestones = 10.0%**.

## Verified Engine-facing key-lock lifecycle boundary

The opt-in `TimeStretchEngineBridge` now has an explicit transactional off-callback staging path for the lifecycle that a future production Engine owner will need.

The qualified boundary now:

- accepts one `ControlSnapshot` containing playback rate, pitch and enable state;
- validates the complete snapshot before changing the research path and fails closed on invalid controls;
- disables/resets stale prefetched state, applies rate/pitch, primes the exact immutable `Clip`/cursor/loop identity and only then enables stretch;
- intentionally keeps a bypassed snapshot unprimed so stale FIFO content cannot resume after production transport has advanced;
- renders the existing production-style pitch-changing hybrid converter in parallel as deterministic fallback;
- preserves existing `StreamCache` starvation/refill episode diagnostics when that fallback path encounters or recovers from missing streamed data;
- exposes algorithm-latency-compensated audible cursor metadata while keeping hardware latency outside the claim;
- keeps prepare/re-prime work outside the measured audio callback;
- covers staged rate/pitch changes, bypass, invalid-control fail-close/recovery, clip/loop/cursor discontinuities, stream failure and fallback refill with deterministic fixtures;
- retains zero heap allocation/deallocation across two separately staged realtime render windows in the warmed Engine-facing contract.

## Validation state

- PR #19 exact-final-head run `35390608930` is green across Linux and Windows development gates.
- Linux completed 15/15 CTest targets under ASan/UBSan, including both Engine-facing integration targets.
- Windows x64 completed configure/build, full CTest, audio diagnostic replay, native no-audio GUI lifecycle smoke, staging and artifact upload.
- Shared-runner timing remains diagnostic only. No physical Windows 11 audio interface, controller, reviewed music-domain listening, measured device latency or hardware-underrun qualification is claimed.
- `docs/progress.json` remains unchanged at 1/10 = 10.0%; this lifecycle qualification does not by itself close M1 or M2.

## Remaining blockers / gates

1. Production `Engine::process()` still uses the current pitch-changing hybrid converter; the verified lifecycle boundary must now be owned by a deck-level production hook before a user-visible key-lock control is enabled.
2. The production hook must prove allocation-free rendering, explicit off-callback prepare/re-prime, bounded fallback, de-clicked transitions and correct transport/meter semantics across load/seek/loop/rate/pitch changes.
3. Algorithm-latency compensation is verified as scheduling metadata, not physical device latency or perceptual switching quality.
4. Clean Windows 11 interactive launch, resize/import and physical two-/four-output audio-interface behavior remain manual M1 gates.
5. Beat/tempo/key analysis, editable beat grids and the remainder of M2 remain open.

## Next highest-impact step

Introduce the smallest deck-owned production selector around the already-qualified Engine-facing bridge, keeping ordinary playback as the default and immediate fallback. The first slice should expose no GUI control yet: it should prove off-callback lifecycle ownership plus allocation-free block rendering and transport/meter correctness under deterministic load/seek/loop/rate/pitch transition tests before the path becomes user-selectable.
