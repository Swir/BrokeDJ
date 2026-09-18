# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`
- Verified `main` baseline before this slice: `db7fc77fa8221f7497af0c6ebac528a3fe987109` (PR #14 merged).
- Active development branch: `feat/timestretch-source-bridge`
- Pull request: `#15` — real `Clip` / `StreamCache` source bridge for the opt-in bounded key-lock research path.
- Implementation commit before this documentation checkpoint: `8e417edf824e0a25d87fe97535f491242a893141`; the commit containing this status file becomes the next PR head and requires its own exact-head CI before merge.
- Initial implementation run `35373507783` reached a green Linux ASan/UBSan build + CTest on `8e417edf...`; Windows x64 was still running when this checkpoint was written. That run is evidence for the implementation commit only, not for the later documentation head.
- Roadmap counter remains: **M0 complete; 1/10 equal-weight milestones = 10.0%**.

## Verified PR #14 baseline

- PR #14 merged the JUCE-independent `TimeStretchDeckAdapter` on top of the earlier pinned Signalsmith prototype.
- The adapter supplies a bounded source/output clock, deterministic fractional source-frame carry, failure/no-advance semantics, seek/load/loop-style reset/preroll handling, pitch controls and a warmed zero-heap deck-level realtime contract.
- Ordinary BrokeDJ playback remains independent of the prototype; the production `Engine` still uses the Catmull-Rom/windowed-sinc pitch-changing rate converter.
- No physical audio-device, controller, reviewed listening, device latency or underrun qualification was claimed by that merge.

## Implemented in PR #15

- Added `TimeStretchSourceBridge`, still JUCE-independent and still opt-in behind `BROKEDJ_BUILD_TIMESTRETCH_PROTOTYPE`.
- The bridge consumes exact bounded input requests from real BrokeDJ `Clip` and lock-free `StreamCache` sources into scratch vectors allocated by `prepare()` rather than growing buffers in render.
- Fractional source cursors are preserved with bounded interpolation instead of being silently rounded to whole frames.
- Whole-track loop reads wrap deterministically. Non-loop EOF, source-rate mismatch, missing cache data and processor failure fail closed without advancing the caller transport.
- Stream misses reuse `StreamCache::noteStarvation()` / `noteRefill()` so research-path failures remain visible through existing diagnostics rather than being hidden as zeros.
- Prime/refill onset uses a prepared short entry fade. This is only a bounded onset safeguard; seamless fallback crossfade against production playback is still open.
- Added deterministic source-bridge tests for 1.25x pitch preservation, fractional transport, loop/EOF behavior, source-rate mismatch, cache starvation/no-advance and refill recovery.
- Added a warmed source-bridge realtime contract with playback-rate automation and a required zero heap allocation/deallocation measured window.

## Validation state

- PR #15 must pass exact-final-head Linux sanitizer and Windows x64 jobs after this checkpoint commit before merge.
- Required Windows coverage remains configure/build, full CTest (including decoder and all opt-in time-stretch targets), verbose audio diagnostic replay, native no-audio GUI lifecycle smoke, staging and artifact upload.
- Shared-runner timing is diagnostic only. No physical Windows 11 audio interface, controller, reviewed music-domain listening, device latency or underrun qualification is claimed.
- `docs/progress.json` remains unchanged at 1/10 = 10.0%; this integration infrastructure does not close M1 or M2.

## Remaining blockers / gates

1. PR #15 exact-final-head Linux + Windows CI must be green before merge.
2. Production key lock still needs an opt-in Engine-facing integration that avoids a `brokedj_core` ↔ optional Signalsmith dependency cycle.
3. The Engine-facing path must resolve clip sample rate versus device output rate explicitly, consume reported time-stretch latency in its clock/FIFO design and crossfade/de-click enable, bypass, seek, loop, load, starvation and processor-failure transitions against the current fallback.
4. Clean Windows 11 interactive launch, resize/import and physical two-/four-output audio-interface behavior remain manual M1 gates.
5. Representative music-domain listening plus explicit Windows CPU/callback-deadline/underrun measurements are required before key lock can become a normal user-facing feature.
6. Beat/tempo/key analysis, editable beat grids and the remainder of M2 remain open.

## Next highest-impact step

Qualify PR #15 on its exact final head. After that, build an opt-in Engine-facing output/FIFO integration around the source bridge: explicit source/output sample-rate conversion, processor-latency compensation and deterministic fallback crossfades first, then seek/loop/load/starvation render fixtures. The existing production resampler stays available until those gates are proven.
