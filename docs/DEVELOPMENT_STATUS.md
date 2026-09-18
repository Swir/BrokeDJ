# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`.
- Verified merged baseline: `4a1f251685d6bde113aac40424b2d30e991a256d` (PR #16 merged).
- PR #16 exact-final-head run `35377183330` completed successfully: Linux ASan/UBSan + prototype CTest passed; Windows x64 configure/build/full CTest/audio diagnostics/native no-audio GUI smoke/staging/artifact upload passed.
- Active development branch: `fix/timestretch-discontinuity-identity`.
- Pull request: `#17` — fail-closed clip/loop/control identity hardening before Engine-facing key-lock integration.
- Implementation/test checkpoint before this status note: `fd6475ca9815adbfd5d4b26eec68cd1fb1061e99`. The live PR head and its exact-head Actions run are authoritative after this note is committed.
- Roadmap counter remains **M0 complete; 1/10 equal-weight milestones = 10.0%**.

## Verified device-rate bridge baseline

PR #16 added the opt-in JUCE-independent `TimeStretchDeviceBridge` above the real `Clip` / `StreamCache` source bridge. It converts stretched source-domain audio to a fixed device rate through a prepared 24-tap / 128-phase band-limited SRC and bounded preallocated FIFO, keeps audible transport separate from prefetch, consumes caller-provided production fallback audio, exposes algorithm-latency metadata and requires explicit re-prime after discontinuities. Production `Engine::process()` remains unchanged and continues using its pitch-changing Catmull-Rom/windowed-sinc converter.

## Implemented in PR #17

- A successful prime is now bound to the exact immutable `Clip` object and loop mode. Clip replacement, loop-mode changes and cursor discontinuities invalidate prefetched research state before it can be rendered.
- Added bounded `FallbackReason` diagnostics for disabled, unprimed, control-change, cursor, clip, loop, source-rate and stretch-failure paths without adding callback logging or heap work.
- Re-applying unchanged playback-rate/pitch snapshots is idempotent; actual control changes invalidate the prepared path and require off-callback re-prime.
- Deterministic tests now cover stale-FIFO rejection across clip replacement, loop-mode changes, cursor jumps, source-rate mismatch and changed controls while verifying the caller's production transport decision is preserved.
- The warmed device-rate realtime contract now pushes unchanged playback-rate/pitch snapshots every measured block and continues to require zero heap allocation/deallocation.

## Validation state

- PR #17 must pass exact-final-head Linux sanitizer and Windows x64 jobs before merge.
- Required Windows coverage remains configure/build, full CTest including all opt-in time-stretch targets, verbose audio diagnostic replay, native no-audio GUI lifecycle smoke, staging and artifact upload.
- Shared-runner timing remains diagnostic only. No physical Windows 11 audio interface, controller, reviewed music-domain listening, measured device latency or underrun qualification is claimed.
- `docs/progress.json` remains unchanged at 1/10 = 10.0%; PR #17 hardens a research boundary and does not close M1 or M2.

## Remaining blockers / gates

1. PR #17 exact-final-head Linux + Windows CI must be green before merge.
2. Production key lock still needs a smallest-possible opt-in Engine-facing integration without creating a mandatory Signalsmith dependency for ordinary playback.
3. Reported stretch/SRC latency must be applied to deck/output scheduling, not merely exposed as metadata.
4. Engine-level parallel-fallback fixtures must cover enable/bypass, rate/pitch change, seek, loop, load, cache starvation and processor failure before any key-lock UI is exposed.
5. Clean Windows 11 interactive launch, resize/import and physical two-/four-output audio-interface behavior remain manual M1 gates.
6. Beat/tempo/key analysis, editable beat grids and the remainder of M2 remain open.

## Next highest-impact step

Qualify PR #17 on its exact final head. If green, implement the smallest opt-in Engine-facing integration around the device bridge: prepare/re-prime outside the callback, render the current production converter in parallel as deterministic fallback, apply algorithm-latency scheduling explicitly, and add transition fixtures before exposing a key-lock control.
