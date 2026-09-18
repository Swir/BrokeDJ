# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`.
- Verified merged baseline: `6b8449932c23ab2e6aafc3ab4bcbcc1e62a42615` (PR #17 merged).
- PR #17 exact-final-head `689fc096c04eeeea889a0d81b32e940b2837e995` passed run `35379038363`: Linux ASan/UBSan + full optional time-stretch CTest passed; Windows x64 configure/build/full CTest/audio diagnostics/native no-audio GUI smoke/staging/artifact upload passed.
- No competing BrokeDJ development PR was left open by this checkpoint.
- Roadmap counter remains **M0 complete; 1/10 equal-weight milestones = 10.0%**.

## Verified device-rate research boundary

PR #16 added the opt-in JUCE-independent `TimeStretchDeviceBridge` above the real `Clip` / `StreamCache` source bridge. It converts stretched source-domain audio to a fixed device rate through a prepared 24-tap / 128-phase band-limited SRC and bounded preallocated FIFO, keeps audible transport separate from prefetch, consumes caller-provided production fallback audio, exposes algorithm-latency metadata and requires explicit re-prime after discontinuities. Production `Engine::process()` remains unchanged and continues using its pitch-changing Catmull-Rom/windowed-sinc converter.

PR #17 then hardened the bridge before any Engine hookup:

- a successful prime is bound to the exact immutable `Clip` object and loop mode;
- clip replacement, loop-mode changes and cursor discontinuities invalidate prefetched research state before it can be rendered;
- bounded `FallbackReason` diagnostics distinguish disabled, unprimed, control-change, cursor, clip, loop, source-rate and stretch-failure paths without callback logging or heap work;
- unchanged playback-rate/pitch snapshots are idempotent while actual control changes require off-callback re-prime;
- deterministic fixtures reject stale FIFO reuse across clip/loop/cursor/source-rate/control discontinuities while preserving caller-provided production transport;
- the warmed device-rate realtime contract pushes unchanged playback-rate/pitch snapshots every measured block and still requires zero heap allocation/deallocation.

## Validation state

- PR #17 exact-final-head Linux and Windows development gates are green and the change is merged.
- Shared-runner timing remains diagnostic only. No physical Windows 11 audio interface, controller, reviewed music-domain listening, measured device latency or underrun qualification is claimed.
- `docs/progress.json` remains unchanged at 1/10 = 10.0%; these research/hardening slices do not close M1 or M2.

## Remaining blockers / gates

1. Production key lock still needs a smallest-possible opt-in Engine-facing integration without creating a mandatory Signalsmith dependency for ordinary playback.
2. Reported stretch/SRC latency must be applied to deck/output scheduling, not merely exposed as metadata.
3. Engine-level parallel-fallback fixtures must cover enable/bypass, rate/pitch change, seek, loop, load, cache starvation and processor failure before any key-lock UI is exposed.
4. Clean Windows 11 interactive launch, resize/import and physical two-/four-output audio-interface behavior remain manual M1 gates.
5. Beat/tempo/key analysis, editable beat grids and the remainder of M2 remain open.

## Next highest-impact step

Implement the smallest opt-in Engine-facing integration around the verified device bridge: prepare/re-prime outside the callback, render the current production converter in parallel as deterministic fallback, apply algorithm-latency scheduling explicitly, and add transition fixtures before exposing a key-lock control.
