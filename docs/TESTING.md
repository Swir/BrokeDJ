# Validation record

## Verified merged baseline: 2026-09-18

Transport-continuity PR #3 was merged as `6075283f3029a78eb587fb09eda98b7246b444d5`; merged-main checkpoint `cc5b43ebbd323d2119c54ff2fea8d8db9fab0199` passed GitHub Actions run `35331491062`.

That baseline includes Linux ASan/UBSan core checks, generated SVG synchronization, Windows Server 2022 / MSVC x64 configure + Release build, core/quality CTest, native no-audio GUI lifecycle smoke and development artifact staging. These checks do not certify physical audio hardware or live reliability.

## Bounded streaming/read-ahead package

PR #4 added a fixed-size stream cache to the JUCE-independent core and a background JUCE read-ahead source for larger local tracks. Small supported tracks retain the in-memory path. The streaming path removes the former fixed 256 MiB decoded-whole-track ceiling without moving file I/O or decoding into the audio callback.

The final PR head `9d842feecda23ea78904a35a9f4c073533784abc` passed GitHub Actions run `35334147477`: Linux sanitizer/core tests and SVG checks succeeded; Windows Server 2022 / MSVC x64 configure/build, all CTest targets including streaming cache, native no-audio GUI smoke, staging and artifact upload also succeeded. PR #4 was then merged to `main` as `ac9a98610a166548c3ca0e18446fc95af99cd7c9`.

Before publication, JUCE-independent C++20 compilation also passed locally and `brokedj_stream_cache_tests` reported **11/11 streaming-cache checks**.

Deterministic streaming coverage verifies:

- a cache miss produces bounded silence and requests the missing source region;
- a published chunk becomes visible and preserves stereo sample data;
- stream-backed clips satisfy core validation and can be submitted to a deck;
- cached streamed audio renders finite, non-zero output through the engine;
- a seek into an uncached region updates the read-ahead request instead of doing callback I/O.

## Seek/refill hardening package

PR #5 extends the same test target and read-ahead implementation without changing the roadmap counter. Exact-final-head CI is required before merge.

The new deterministic coverage uses a **virtual 90-minute stream** backed only by the fixed cache; it does not allocate or decode a 90-minute audio buffer. It verifies:

- a non-audio `StreamCacheDiagnostics` snapshot reports the exact requested chunk, whether that region is resident and bounded forward cache coverage;
- repeated distant seeks across the virtual long track keep requested frames in range, output finite and the playhead valid;
- a streamed whole-track loop can wrap from the final region back to a preloaded start region while remaining in the playing state;
- an intentionally unfilled seek target is visible as cache starvation and becomes ready after deterministic refill;
- the background reader prioritizes the exact requested chunk before forward look-ahead after a seek;
- chunks beyond EOF are skipped by the read-ahead scheduler instead of being counted as work, preventing a potential end-of-track busy loop.

These cache diagnostics describe **read-ahead residency**, not physical audio-interface underruns. A ready requested chunk also does not prove that every Catmull-Rom neighbor, storage device, compressed codec or OS scheduling sequence is dropout-free.

The tests deliberately do **not** claim zero dropouts after arbitrary seeks. Cache-refill onset, slow-storage behavior, compressed-codec seeking, long-file loop edges on real files and worker shutdown during blocked I/O still need explicit stress/listening evidence.

## Audio quality hardening coverage

`brokedj_quality_tests` covers stable playback, pause/seek transitions, loop-wrap continuity, cue switching continuity and control automation. These tests support short transport/cue transitions and smoothing. They do **not** prove inaudibility on every file, buffer size, device or loudspeaker chain.

## Existing core coverage

Empty silence; invalid clip/deck/device-rate rejection; stereo playback; playhead/rate/seek behavior; crossfader endpoints; cue isolation on two/four outputs; EOF; whole-track looping; clipping/finite values; three-band kill; an echo impulse; stopped replacement; retirement backpressure; concurrent publication and shutdown.

## What automated checks do not certify

Automated CI does not by itself certify Windows 11 clean-machine usability, physical audio hardware, device switching, real four-output cue isolation, controller support, latency, listening quality, zero audible clicks/dropouts, ASIO support, or multi-hour live reliability. Catmull-Rom rate conversion is still pitch-changing resampling, not key lock/time stretch.

## Native acceptance checklist

- [x] Windows x64 build and native no-audio smoke mode pass on the merged baseline.
- [x] SVG progress synchronization and no-legacy-meter check pass on the merged baseline.
- [x] Bounded streaming/read-ahead PR passes exact-head Linux + Windows CI and is merged.
- [ ] Seek/refill hardening PR #5 passes exact-final-head Linux + Windows CI and is merged.
- [ ] Clean Windows 11 machine launches and logs startup correctly.
- [ ] Mono/stereo WAV, FLAC, OGG, AIFF, CBR/VBR MP3 fixtures decode/stream as expected.
- [ ] Invalid, truncated and Unicode-path files fail clearly without losing working audio.
- [ ] Large/long files stay memory-bounded under repeated seek/loop/load stress on real supported codecs/storage.
- [ ] Minimum and large window sizes keep every control reachable.
- [ ] Two-output and four-output physical devices have correct master/cue isolation.
- [ ] Device change/disconnect and closing during decode/streaming recover safely.
- [ ] Sample-rate/buffer changes are exercised without invalid output or transport corruption.
- [ ] Long-running simultaneous playback and repeated loading pass an agreed soak test.
- [ ] Transport/seek/loop/cue/cache transitions receive deterministic metrics plus reviewed listening checks on representative fixtures.

Use original/generated or appropriately licensed audio fixtures only. Report commit, OS, device/driver, sample rate, buffer size, reproduction steps and a reviewed/redacted log. Do not mark a release gate complete based only on a scheduled run, screenshot or compile result.
