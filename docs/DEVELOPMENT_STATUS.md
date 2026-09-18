# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`.
- Verified merged baseline: `2a9647466375f071b47a0c2ae1f47be2adf5fb56` after the PR #17 discontinuity/identity gate was recorded.
- Active work: PR #18 on `feat/engine-facing-keylock-adapter` qualifies the smallest opt-in Engine-facing boundary around the verified time-stretch/device bridge; it still does not replace `Engine::process()` or expose a key-lock UI control.
- The first PR #18 exact-head run `35381874230` compiled successfully on Linux and Windows but failed the new `time_stretch_engine_bridge` test on both platforms at `Engine-facing bridge prepares`. The failure was deterministic, not a runner-only timeout.
- Root cause: the deck adapter sized prepared input only from maximum realtime output demand. At 96 kHz with the 96->48 kHz / 256-device-frame Engine fixture, Signalsmith discontinuity/seek history can exceed that block-derived capacity, causing `TimeStretchSourceBridge::prepare()` to reject an otherwise supported source/device-rate pair.
- Candidate fix on PR #18 now discovers Signalsmith seek history during off-callback `prepare()`, widens the prepared input capacity when required, and adds a direct 96 kHz small-block regression fixture while retaining the existing absolute bounds. Exact-final-head CI remains the merge gate.
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

## PR #18 Engine-facing qualification scope

The active candidate adds `TimeStretchEngineBridge` as an opt-in integration boundary. It renders the existing production-style hybrid source converter in parallel as deterministic fallback, feeds that fallback into the bounded stretch/device bridge, exposes an algorithm-latency-compensated audible cursor for future scheduling, and adds Engine-level transition/failure/realtime fixtures. It remains research-only until the exact final PR head passes the full required CI matrix and is merged.

The high-rate preparation fix is deliberately confined to off-callback preparation. Runtime `inputFramesForOutput()` and `processStereo()` remain bounded by prepared capacity, while discontinuity history now fits the same capacity contract instead of relying on a sample-rate-specific guessed preroll constant.

## Validation state

- PR #17 exact-final-head Linux and Windows development gates are green and merged.
- PR #18 initial head failed deterministically in the new high-rate Engine bridge fixture; the root cause is diagnosed and a bounded capacity-planning fix plus regression coverage is now on the same branch for exact-final-head revalidation.
- Shared-runner timing remains diagnostic only. No physical Windows 11 audio interface, controller, reviewed music-domain listening, measured device latency or underrun qualification is claimed.
- `docs/progress.json` remains unchanged at 1/10 = 10.0%; Engine-facing key-lock research does not by itself close M1 or M2.

## Remaining blockers / gates

1. PR #18 must pass exact-final-head Linux ASan/UBSan and Windows x64 build/full CTest/GUI-smoke/development packaging before merge.
2. Production `Engine::process()` still uses the current pitch-changing hybrid converter; key lock remains opt-in research until the integration boundary is fully qualified and deliberately wired.
3. Algorithm-latency compensation is being qualified as scheduling metadata; real device latency and perceptual switching still require hardware/listening evidence.
4. Clean Windows 11 interactive launch, resize/import and physical two-/four-output audio-interface behavior remain manual M1 gates.
5. Beat/tempo/key analysis, editable beat grids and the remainder of M2 remain open.

## Next highest-impact step

Finish PR #18 exact-head qualification. If green, merge the Engine-facing boundary without exposing UI, then use the verified adapter to design the smallest safe production hookup with explicit off-callback prepare/re-prime ownership and deterministic bypass/fallback behavior before any user-visible key-lock control is enabled.
