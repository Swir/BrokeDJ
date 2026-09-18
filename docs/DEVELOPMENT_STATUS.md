# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`.
- Verified merged baseline: `a3560581bf63f5a2cc75a8519f3e07c9dd9580f9` (PR #15 merged).
- Baseline merged-main run `35375177563` completed successfully across Linux sanitizer/core checks and the Windows x64 development gate.
- Active development branch: `feat/timestretch-device-bridge`.
- Pull request: `#16` — explicit source-rate → device-rate bridge and deterministic production-fallback boundary for the opt-in key-lock research path.
- Implementation/documentation checkpoint before this status note: `5eb7484ec5f79f953513aab3cd70fe84a4dd0e3e`. The live PR head and its exact-head Actions run are authoritative after this note is committed.
- Roadmap counter remains **M0 complete; 1/10 equal-weight milestones = 10.0%**.

## Verified PR #15 baseline

- `TimeStretchSourceBridge` consumes exact bounded requests from real BrokeDJ `Clip` and lock-free `StreamCache` sources using scratch allocated by `prepare()`.
- Fractional source cursors, loop wrapping, source-rate checks, stream starvation/refill diagnostics and fail-closed/no-advance behavior are covered by deterministic tests.
- A warmed source-bridge realtime contract requires zero heap allocation/deallocation in the measured render window.
- Production `Engine::process()` remains independent and continues using the Catmull-Rom/windowed-sinc pitch-changing converter.

## Implemented in PR #16

- Added JUCE-independent `TimeStretchDeviceBridge`, still opt-in behind `BROKEDJ_BUILD_TIMESTRETCH_PROTOTYPE` and not wired into production `Engine::process()`.
- Added an explicit source-rate/device-rate clock boundary with a bounded preallocated stretched-output FIFO and prepared 24-tap / 128-phase band-limited sample-rate converter.
- Audible transport advancement is kept separate from processor/FIFO prefetch, preventing prefetch depth from silently moving the user-visible deck clock.
- The bridge consumes caller-provided production fallback samples. Disabled, unprimed, discontinuous or failed research rendering preserves the fallback transport decision instead of emitting silence or stale stretch data.
- Bypass and control/discontinuity changes invalidate prefetched research audio and require explicit re-prime before stretch resumes.
- Added deterministic 44.1→48 kHz 1.25x pitch-lock/transport coverage, 96→48 kHz passband/stopband checks, fallback/discontinuity tests, algorithm-latency metadata and a warmed 600-block device-rate zero-heap realtime contract.

## Validation state

- PR #16 must pass exact-final-head Linux sanitizer and Windows x64 jobs before merge.
- Required Windows coverage remains configure/build, full CTest including all opt-in time-stretch targets, verbose audio diagnostic replay, native no-audio GUI lifecycle smoke, staging and artifact upload.
- Shared-runner timing is diagnostic only. No physical Windows 11 audio interface, controller, reviewed music-domain listening, device latency or underrun qualification is claimed.
- `docs/progress.json` remains unchanged at 1/10 = 10.0%; this research integration does not close M1 or M2.

## Remaining blockers / gates

1. PR #16 exact-final-head Linux + Windows CI must be green before merge.
2. Production key lock still needs an opt-in `Engine::process()` integration without creating a `brokedj_core` ↔ optional Signalsmith dependency cycle.
3. Reported stretch/SRC latency must be applied to deck/output scheduling, not merely exposed as metadata.
4. Enable/bypass, rate/pitch change, seek, loop, load, cache starvation and processor failure still need Engine-level parallel-fallback render fixtures and reviewed listening.
5. Clean Windows 11 interactive launch, resize/import and physical two-/four-output audio-interface behavior remain manual M1 gates.
6. Beat/tempo/key analysis, editable beat grids and the remainder of M2 remain open.

## Next highest-impact step

Qualify PR #16 on its exact final head. If green, the next code slice should be the smallest opt-in Engine-facing integration: prepare/re-prime outside the callback, render current production playback in parallel as deterministic fallback, apply algorithm-latency scheduling explicitly, and add transition fixtures before any key-lock UI is exposed.
