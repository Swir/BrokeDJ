# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`.
- PR #21 (`feat/engine-deck-source-hook`) merged as `1e30cb3ff352d7c6ded8751d6bb1757be77e1911` after exact-final-head `64371683f7e8447e5a82044164c826ad26f17101` passed GitHub Actions run `35401027839`.
- That exact-head gate passed Linux ASan/UBSan with **20/20 CTest targets**, including generic Engine source integration, key-lock discontinuity/fallback integration and the warmed full Engine + key-lock realtime contract, plus Windows x64 configure/build/full CTest/audio diagnostics/native no-audio GUI smoke/staging/artifact upload.
- The optional key-lock path can now enter `Engine` before the existing production deck DSP/routing chain without making time-stretch a mandatory `brokedj_core` dependency or exposing a GUI control.
- Roadmap counter remains **M0 complete; 1/10 equal-weight milestones = 10.0%**.

## Verified production Engine source boundary

PR #21 adds a bounded, non-owning `DeckSourceRenderer` boundary to `Engine` and an optional `EngineKeyLockSource` adapter above the already-qualified `DeckPlaybackSelector`.

The production-facing path now:

- preallocates per-deck stereo source scratch in `Engine::prepare()` and refuses unbounded block sizes;
- invokes an optional source exactly once per deck block, before the existing EQ, drive, echo, gain, cue, crossfader, master protection and peak-meter stages;
- immediately falls back to the built-in production rate converter when an external source refuses a block;
- keeps transport and algorithm-latency-compensated audible meter positions separate when the qualified key-lock path is active;
- preserves four-output cue routing and the existing production protection/meter semantics for externally rendered deck audio;
- binds the key-lock stage to the exact immutable clip, loop mode and playback-rate target, failing closed after clip/loop/rate drift until explicit off-callback restaging;
- lets seek/cursor discontinuities remain inside the qualified selector boundary, which rejects stale stretch history and emits the prepared production-style fallback instead;
- retains a zero-heap callback contract across the full Engine + key-lock path after prepare/stage.

## Validation state

- PR #21 exact-final-head run `35401027839` is green across Linux and Windows development gates.
- Linux completed 20/20 CTest targets under ASan/UBSan; `engine_keylock_source_realtime` exercised 300 warmed Engine blocks while requiring zero heap allocation/deallocation in the measured callback window.
- Windows x64 completed configure/build, full CTest, expanded audio diagnostic replay including the new Engine key-lock targets, native no-audio GUI lifecycle smoke, staging and artifact upload.
- Shared-runner timing remains diagnostic only. No physical Windows 11 audio interface, controller, reviewed music-domain listening, measured device latency or hardware-underrun qualification is claimed.
- `docs/progress.json` remains unchanged at 1/10 = 10.0%; this integration boundary does not close M1 or the broader M2 performance-deck milestone.

## Remaining blockers / gates

1. The JUCE/app ownership layer does not yet create/manage one qualified key-lock source per deck or serialize prepare/restage/disarm around device, clip and transport mutations.
2. The production hook has deterministic de-click/fallback coverage, but reviewed music-domain listening for perceptual switching quality remains open.
3. Algorithm-latency compensation is scheduling metadata, not physical device latency.
4. Clean Windows 11 interactive launch, resize/import and physical two-/four-output audio-interface behavior remain manual M1 gates.
5. BPM/tempo/key analysis, editable beat grids, hotcues, beat loops, slip/reverse/scratch and the remainder of M2 remain open.

## Next highest-impact step

Add the smallest app/deck ownership controller that prepares one `EngineKeyLockSource` per loaded deck outside the callback, stages/restages it only at safe device/clip/transport boundaries, and keeps ordinary production playback as immediate fallback. Qualify load/device-rate/seek/loop/rate/pitch lifecycle races and shutdown before exposing any user-facing key-lock control; then move into real BPM/beat-grid analysis.
