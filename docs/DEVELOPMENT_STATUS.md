# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`.
- PR #18 (`feat/engine-facing-keylock-adapter`) merged as `bce596c88c32ccea7244ad7a7036cc929cb39ccd` after exact-final-head `3e2e92745cbed6e194a22b687db477899776d75a` passed GitHub Actions run `35384529676`.
- That exact-head gate passed Linux ASan/UBSan plus all 15 core/time-stretch CTest targets, and Windows x64 configure/build/full CTest/audio diagnostics/native no-audio GUI smoke/staging/artifact upload.
- The earlier PR #18 run `35381874230` had failed deterministically on Linux and Windows at `Engine-facing bridge prepares`. The root cause was a real 96 kHz capacity-planning bug, not a transient runner error.
- The fix now sizes prepared deck input for both worst-case realtime demand and Signalsmith discontinuity/seek history during off-callback `prepare()`, with a dedicated 96 kHz small-block regression fixture.
- Roadmap counter remains **M0 complete; 1/10 equal-weight milestones = 10.0%**.

## Verified Engine-facing key-lock research boundary

PR #18 adds `TimeStretchEngineBridge` as the smallest opt-in boundary above the already-qualified source/device bridge stack. It does not replace ordinary `Engine::process()` and does not expose a key-lock UI control.

The integration candidate:

- renders the existing production-style pitch-changing hybrid converter in parallel as deterministic fallback;
- feeds that fallback into the bounded time-stretch/device-rate bridge instead of emitting stale research audio on failure;
- exposes an algorithm-latency-compensated audible transport cursor for future deck/sync scheduling;
- keeps prepare/re-prime work outside the audio callback;
- covers enable/bypass, rate/pitch changes, seek, loop, load, stream failure and fallback behavior with deterministic fixtures;
- retains a warmed Engine-facing realtime contract with zero heap allocation/deallocation in the measured render window.

The high-rate preparation correction is also merged: `TimeStretchDeckAdapter::prepare()` first configures the processor to discover its sample-rate-dependent seek history, then widens the prepared input bound when that history exceeds ordinary block demand. The absolute frame bound is preserved, and runtime render requests remain bounded by the prepared capacity.

## Validation state

- PR #18 exact-final-head run `35384529676` is green across Linux and Windows development gates.
- Merged code is now on `main`; the normal post-merge `main` workflow remains the source of truth for the final merged snapshot.
- Shared-runner timing remains diagnostic only. No physical Windows 11 audio interface, controller, reviewed music-domain listening, measured device latency or underrun qualification is claimed.
- `docs/progress.json` remains unchanged at 1/10 = 10.0%; this integration research does not by itself close M1 or M2.

## Remaining blockers / gates

1. Production `Engine::process()` still uses the current pitch-changing hybrid converter; the verified adapter must be wired deliberately with explicit off-callback lifecycle ownership before user-visible key lock is enabled.
2. Algorithm-latency compensation is verified as scheduling metadata, not physical device latency or perceptual switching quality.
3. Clean Windows 11 interactive launch, resize/import and physical two-/four-output audio-interface behavior remain manual M1 gates.
4. Beat/tempo/key analysis, editable beat grids and the remainder of M2 remain open.

## Next highest-impact step

Design the smallest safe production hookup that lets a deck opt into the verified Engine-facing key-lock bridge while preserving the current converter as immediate fallback, with explicit prepare/re-prime ownership and deterministic transition tests before any GUI control is enabled.
