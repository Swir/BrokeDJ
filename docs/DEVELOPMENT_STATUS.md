# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`
- Verified `main` baseline: `9cac4f400774354d383a7288a9e3bd9ffdb790d6` (PR #13 merged)
- `main` validation run `35368603454`: Linux ASan/UBSan core qualification and Windows x64 configure/build/full CTest/audio diagnostics/native no-audio GUI smoke/staging/artifact upload passed.
- Active development branch: `feat/keylock-deck-clock`
- Open pull request: `#14` — bounded deck-side clock/buffer contract for the opt-in key-lock research path
- Implementation head immediately before this status write: `e2c2ac74c0aa015131b2025589275bf1e80886f4`; the commit containing this status file becomes the next PR head and requires its own exact-head CI before merge.
- Roadmap counter remains: **M0 complete; 1/10 equal-weight milestones = 10.0%**.

## Verified PR #13 baseline

- Added an opt-in JUCE-independent `TimeStretchPrototype` using pinned Signalsmith Stretch (`57b93f4e9206a089a45387eaa39bdc9f310d3308`) and Signalsmith Linear (`5668673560146a9cfe38c25315071e3fd68c8317`) after MIT-license review.
- Ordinary BrokeDJ playback remains independent of the prototype; the production `Engine` still uses the Catmull-Rom/windowed-sinc pitch-changing rate converter.
- Deterministic tests cover bounded input/output, 1.25x time-ratio pitch preservation, explicit pitch shift, latency metadata, reset/seek and warmed zero-heap processing.
- PR #13 exact-final-head run `35367977696` passed before merge, and merged-main run `35368603454` independently passed the complete Linux + Windows development gate.

## Implemented in PR #14

- Added `TimeStretchDeckAdapter`, still JUCE-independent and still not connected to production `Engine` playback.
- Added a bounded source/output clock contract: a future deck integration can ask exactly how many contiguous source frames are needed for a fixed output block at the current playback rate.
- Added normalized fractional source-frame carry so non-integer rates do not accumulate whole-frame clock drift across many blocks.
- Failed or mismatched blocks leave source-consumption and fractional-clock state unchanged, preserving a clean immediate-fallback boundary to the existing production rate converter.
- Added seek/load/loop-style discontinuity reset/preroll semantics without doing disk I/O, decoding or dynamic buffer growth in the adapter.
- Added deterministic deck-clock coverage for 1.25x pitch preservation, fractional 1.001x source-clock drift, failure/no-advance behavior, bounds, pitch controls and discontinuity reset.
- Added a warmed deck-level realtime contract with rate automation among 1.0x/1.25x/1.5x, finite output checks and zero heap allocation/deallocation requirements in the measured window.

## Validation state

- PR #14 CI is intentionally treated as incomplete until both Linux sanitizer and Windows x64 jobs are green on the exact newest PR head, including any documentation checkpoint commit.
- An earlier PR #14 run already reached a green Linux sanitizer job, but it is not sufficient evidence for a later head.
- Windows qualification must include configure/build, full CTest (including decoder, prototype and deck-clock targets), verbose audio diagnostic replay, native no-audio GUI lifecycle smoke, staging and artifact upload.
- Shared-runner timing remains diagnostic only. No physical Windows 11 audio interface, controller, reviewed music-domain listening, device latency or underrun qualification is claimed.
- `docs/progress.json` remains unchanged at 1/10 = 10.0%; this integration infrastructure does not close M1 or M2.

## Remaining blockers / gates

1. PR #14 must pass exact-final-head Linux + Windows CI before merge.
2. Production key lock still needs an Engine-facing bounded source/FIFO integration, explicit latency compensation and de-clicked enable/bypass/failure fallback around the existing playback path.
3. Seek, loop wrap, clip replacement and streamed-cache starvation/refill need deterministic render coverage with the deck adapter connected behind an opt-in integration boundary.
4. Clean Windows 11 interactive launch, resize/import and physical two-/four-output audio-interface behavior remain manual M1 gates.
5. Representative music-domain listening plus explicit Windows CPU/callback-deadline/underrun measurements are required before key lock can become a normal user-facing feature.
6. Beat/tempo/key analysis, editable beat grids and the remainder of M2 remain open.

## Next highest-impact step

Finish PR #14's exact-final-head qualification and merge only after both required jobs are green. The next engineering slice is an opt-in Engine-side source/FIFO integration that consumes this bounded deck clock, compensates reported processing latency, de-clicks enable/bypass/seek/loop/load transitions and falls back immediately to the current rate converter on starvation or processor failure.
