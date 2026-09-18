# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`
- Active development branch: `feat/time-stretch-prototype`
- Open pull request: `#13` — bounded time-stretch/key-lock research path
- Main baseline before this PR: `2550a142f48ce81d4ad4829e3400cfc9c3f76a60`
- Implementation head with first successful Linux sanitizer qualification: `0532a05d2c9cc13fa4b51a701ce4a308caf88ea7`
- Documentation/checkpoint head immediately before this status write: `4e5a62ed64399dae2e0a4fc9e11f28a65330c090`; the commit containing this status file is the next PR head and requires its own exact-head CI before merge
- Validation run `35367465361`: Linux ASan/UBSan build plus all seven core-only CTest targets passed at implementation head `0532a05d...`; the run is not sufficient for merge after later documentation commits, and Windows/final-head validation must be read from the newest PR run
- Roadmap counter remains: **M0 complete; 1/10 equal-weight milestones = 10.0%**

## Implemented in PR #13

- Added an opt-in JUCE-independent `TimeStretchPrototype` using pinned Signalsmith Stretch (`57b93f4e9206a089a45387eaa39bdc9f310d3308`) and Signalsmith Linear (`5668673560146a9cfe38c25315071e3fd68c8317`) after MIT-license review.
- Kept ordinary BrokeDJ playback independent of the prototype: `Engine` is unchanged and the existing Catmull-Rom/windowed-sinc pitch-changing rate converter remains the production fallback.
- Added prepared bounded input/output sizes, ±24-semitone pitch bounds, latency/seek metadata, reset and seek-preroll support.
- Added deterministic 1.25x key-lock and +12-semitone pitch fixtures plus invalid-input, finite-output, reset and seek tests.
- Added a warmed realtime-contract test requiring zero heap allocations/deallocations across 600 bounded 1.25x processing blocks with periodic pitch changes.
- Added Linux sanitizer and Windows x64 CI qualification for the optional path, offline source-preparation instructions, architecture/testing documentation and third-party notices.
- Diagnosed the initial Linux integration failure to Signalsmith Linear 0.3.1 using `std::memcpy` without including `<cstring>` under GCC/libstdc++; BrokeDJ now provides the standard header before the pinned dependency without patching upstream source.

## Validation state

- Run `35367465361` proved the corrected prototype compiles under GCC 13.3 with ASan/UBSan and passes all seven core-only tests at `0532a05d...`.
- The PR has received additional documentation commits after that implementation head. Do **not** merge or claim full validation until both Linux and Windows jobs are green for the exact newest PR head.
- Windows qualification must include configure/build, full CTest (including decoder and the two time-stretch targets), verbose audio diagnostic replay, native no-audio GUI lifecycle smoke, staging and artifact upload.
- Shared-runner processing timing is diagnostic only; no physical Windows 11 audio interface, controller, reviewed listening, device latency or underrun qualification is provided by this package.
- `docs/progress.json` remains unchanged at 1/10 = 10.0%; prototype infrastructure does not close M1 or M2.

## Remaining blockers / gates

1. Exact-final-head PR #13 Linux + Windows CI must be green before merge.
2. Clean Windows 11 interactive launch, resize/import and physical audio-interface behavior remain manual M1 gates.
3. Production key lock still needs a bounded deck-side source/output clock and buffering model, latency compensation, seek/loop/load reset semantics, de-clicked enable/bypass transitions and immediate fallback behavior.
4. Representative music-domain listening plus explicit Windows CPU/callback-deadline/underrun measurements are required before promoting the prototype into normal playback.
5. Broader real-world codec/storage stress, beat/tempo/key analysis, editable grids and the remainder of M2 remain open.

## Next highest-impact step

First finish PR #13's exact-final-head CI and merge only if both required jobs are green. After merge, design and test the bounded deck-side clock/buffer/latency integration behind an opt-in key-lock switch while preserving the current production rate converter as an immediate fallback. Add deterministic de-click/seek/loop/load render fixtures before exposing any user-facing key-lock control.
