# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`.
- PR #22 (`feat/keylock-deck-owner-handoff`) merged as `062c3e1b1c052c2ce2f4aae523813aed3e2da975` after exact-final-head `6fd486200d376162f7f3660067994c7b5f5129bf` passed GitHub Actions run `35404959160`.
- That exact-head gate passed Linux ASan/UBSan with the full optional time-stretch/key-lock CTest matrix and Windows x64 configure/build/full CTest/audio diagnostics/native no-audio GUI smoke/staging/artifact upload.
- The new `EngineKeyLockDeckOwner` provides a bounded two-slot handoff so a serialized non-audio owner can prepare/stage a replacement key-lock snapshot without mutating the source currently visible to an in-flight callback.
- Roadmap counter remains **M0 complete; 1/10 equal-weight milestones = 10.0%**.

## Verified deck-owned key-lock handoff

PR #22 builds on the production `DeckSourceRenderer` / `EngineKeyLockSource` path qualified in PR #21.

The owner boundary now:

- keeps two preallocated `EngineKeyLockSource` slots and publishes only a fully prepared/staged inactive slot;
- uses an atomic active-slot publication plus a bounded callback hazard index so the non-audio owner does not overwrite a slot retained by an in-flight render;
- keeps staging non-blocking with respect to the audio callback: a protected inactive slot returns `busy` so the owner can retry on its normal control/timer pass;
- supports fail-closed `disarm()` while audio is active, making the next callback use Engine's built-in production rate converter immediately;
- validates device bounds before resetting stopped-audio state and avoids mutating unprepared processor instances during teardown;
- preserves exact immutable clip identity, loop/rate checks, algorithm-latency-compensated audible scheduling and the existing EQ/FX/gain/cue/crossfader/master/meter chain downstream;
- adds deterministic owner handoff tests for configuration, stage, pitch restage, disarm/fallback, disabled state, replacement clips and stopped-audio reset;
- extends the warmed full Engine realtime contract with the owner wrapper while retaining zero heap allocation/deallocation in the measured callback window.

## Validation state

- PR #22 exact-final-head run `35404959160` is green across Linux and Windows development gates.
- Linux sanitizer validation passed the complete configured CTest suite, including the expanded Engine key-lock source/owner functional and realtime coverage.
- Windows x64 completed configure/build, full CTest, audio diagnostic replay, native no-audio GUI lifecycle smoke, staging and artifact upload on the same exact head.
- The earlier intermediate PR #22 workflow was cancelled automatically after the final hardening commit; it is not used as evidence.
- Shared-runner timing remains diagnostic only. No physical Windows 11 audio interface, controller, reviewed music-domain listening, measured device latency or hardware-underrun qualification is claimed.
- `docs/progress.json` remains unchanged at 1/10 = 10.0%; this lifecycle foundation does not close M1 or the broader M2 performance-deck milestone.

## Remaining blockers / gates

1. The JUCE/app ownership layer still does not instantiate/manage the new owner for each deck, because the time-stretch stack remains opt-in and no user-visible key-lock control is qualified yet.
2. Device prepare/release, asynchronous clip load, seek, loop and live rate/pitch intent must be serialized into explicit owner stage/disarm requests without blocking the callback.
3. Reviewed music-domain listening for key-lock quality and perceptual path switching remains open.
4. Algorithm-latency compensation is scheduling metadata, not physical device latency.
5. Clean Windows 11 interactive launch, resize/import and physical two-/four-output audio-interface behavior remain manual M1 gates.
6. BPM/tempo/key analysis, editable beat grids, hotcues, beat loops, slip/reverse/scratch and the remainder of M2 remain open.

## Next highest-impact step

Integrate `EngineKeyLockDeckOwner` into the opt-in native app lifecycle without exposing a GUI control yet: configure/reset owners only at audio-device prepare/release, stage the exact immutable clip after asynchronous load/adoption, translate seek/loop/rate/pitch changes into serialized non-audio restage/disarm work, and qualify shutdown/device-change races plus immediate production fallback. After that lifecycle is deterministic, move into real BPM/beat-grid analysis rather than adding cosmetic controls.
