# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`.
- PR #23 (`feat/native-keylock-lifecycle`) merged as `3ca45e91753004d12be43af97363f28fd85ba42d` after exact-final-head `5cf8859cfe43084e1d68c966674b7bb5c9a5d48b` passed GitHub Actions run `35409039143`.
- That exact-head gate passed Linux ASan/UBSan with **21/21** configured CTest targets and Windows x64 configure/build/full CTest/audio diagnostics/native no-audio GUI smoke/staging/artifact upload.
- The first PR #23 run (`35408750219`) correctly failed a new lifecycle test because the test tried to assert render acceptance before the ordinary Engine clip-adoption callback had completed. The fixture was corrected to model the real submit → callback adoption → paused stage → play order; no failing run is used as release evidence.
- The native app now has a developer-only, compile-time-plus-command-line opt-in lifecycle for the previously qualified owner path. Default builds and default launches keep ordinary production playback.
- Roadmap counter remains **M0 complete; 1/10 equal-weight milestones = 10.0%**.

## Verified native key-lock lifecycle

PR #23 builds on the production `DeckSourceRenderer` / `EngineKeyLockSource` / `EngineKeyLockDeckOwner` path qualified through PRs #21 and #22.

The new application boundary now:

- provides a JUCE-independent `KeyLockDeckLifecycle` that owns the four `EngineKeyLockDeckOwner` instances while remaining deterministically testable outside the GUI;
- configures and removes those owners only at stopped-audio prepare/release boundaries, and leaves the ordinary Engine converter available as the immediate fallback;
- associates a successfully submitted immutable `Clip` with the matching deck lifecycle without transferring Clip ownership into the optional renderer;
- stages/re-primes from the serialized non-audio owner path rather than from the audio callback;
- exposes the real JUCE application integration only when both `BROKEDJ_BUILD_TIMESTRETCH_PROTOTYPE=ON` and the developer-only `--key-lock-research` launch flag are used;
- immediately disarms on live seek, whole-track-loop or playback-rate changes; while playback continues, those changes remain on the existing production converter instead of mutating/re-priming the research DSP in the callback;
- permits deterministic paused restaging before play resumes, including explicit seek target binding and validated pitch intent in the lifecycle layer;
- preserves the existing downstream EQ/drive/echo/gain/cue/crossfader/master/meter path because the optional renderer still enters through Engine's qualified raw-source boundary;
- adds a dedicated lifecycle regression test covering submit/adoption order, stage/play, live rate fallback, paused restage, seek, pitch validation, stopped-audio release and device re-prepare.

## Validation state

- PR #23 exact-final-head run `35409039143` is green across Linux and Windows development gates.
- Linux sanitizer validation passed all 21 configured CTest targets, including the new native lifecycle fixture plus the existing time-stretch, selector, Engine source and realtime contracts.
- Windows x64 completed configure/build, full CTest, audio diagnostic replay, native no-audio GUI lifecycle smoke, staging and artifact upload on the same exact head; this also proves the real JUCE application compiles with the optional research stack linked.
- The ordinary no-audio smoke still launches without the developer flag; `--key-lock-research` is documented as a development-only opt-in, not a product control.
- Shared-runner timing remains diagnostic only. No physical Windows 11 audio interface, controller, reviewed music-domain listening, measured device latency or hardware-underrun qualification is claimed.
- `docs/progress.json` remains unchanged at 1/10 = 10.0%; this integration does not close M1 or the broader M2 performance-deck milestone.

## Remaining blockers / gates

1. Physical Windows 11 clean-machine launch, real audio-device switching and physical two-/four-output cue still block M1 completion.
2. The developer key-lock lifecycle intentionally defers re-prime while a deck is playing; seamless live rate/seek/loop transitions are not qualified and no key-lock GUI control should be exposed yet.
3. App shutdown/device-reprepare/load-replacement race coverage should be strengthened before relaxing that conservative paused-restage rule.
4. Reviewed music-domain listening for key-lock quality and perceptual path switching remains open.
5. Algorithm-latency compensation is scheduling metadata, not measured physical device latency.
6. True BPM/tempo/key analysis, editable beat grids, hotcues, beat loops, slip/reverse/scratch and the remainder of M2 remain open.

## Next highest-impact step

Harden the just-integrated lifecycle around shutdown/device re-prepare and rapid clip replacement, extend the native developer smoke boundary without enabling a product control, then start a real offline BPM/beat-grid analysis/cache subsystem so M2 begins producing musical metadata rather than more integration scaffolding. The physical M1 audio-interface checks remain a separate manual gate.
