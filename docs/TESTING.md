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

## Seek/refill and starvation hardening

PR #5 added deterministic virtual long-track seek/loop/refill coverage and prioritized the exact requested cache region before forward read-ahead. It passed exact-final-head Linux + Windows validation and merged as `7cced0d18fe7d192fa983b482d2637b2204fe0f5`.

PR #6 added lock-free starvation/refill episode counters, readiness-aware Catmull-Rom tap handling and short fade transitions into/out of cache starvation. Final head `915441c73cc6bab7c58579a8343952ca9e1a9bf1` passed run `35338665653` and merged as `58ecdb3b85b8ff395d115097776d5946bf457936`.

The deterministic streaming stress uses a **virtual 90-minute stream** backed only by the fixed cache; it does not allocate or decode a 90-minute audio buffer. It verifies repeated distant seeks, a prepared whole-track loop edge, intentional starvation/refill recovery, event accounting and bounded forward cache coverage.

These cache diagnostics describe **read-ahead residency/playback starvation**, not physical audio-interface underruns. A ready requested chunk also does not prove that every storage device, compressed codec or OS scheduling sequence is dropout-free.

## Decoder / codec stress package

PR #7 makes the JUCE decoder/read-ahead adapter an independent test target. It uses generated original WAV, AIFF, FLAC and OGG fixtures plus an embedded original synthetic MP3 fixture. Production defaults remain unchanged: tracks above the 64 MiB decoded-stereo threshold use streaming and the artificial reader delay stays zero outside tests.

Coverage includes:

- in-memory and forced-streaming decode through the same production adapter;
- Unicode-path AIFF import;
- real JUCE WAV, AIFF, FLAC, OGG and MP3 readers;
- distant cache refill after seek for generated codecs that exceed the prime window;
- invalid-file diagnostics and cancellation before publication;
- controlled slow-reader delay plus last-request-wins read-ahead preemption;
- starvation/refill diagnostics under intentionally delayed background input;
- non-finite streamed sample sanitization before cache publication.

The first two Windows PR runs exposed an MP3 forced-streaming regression that Linux core/sanitizer checks could not reveal. The failing path was kept red and unmerged. The decoder now treats waveform generation and streaming playback as independent reader lifetimes; if a compressed reader rejects the sparse non-monotonic preview pattern, waveform construction falls back to a bounded-memory sequential pass on a fresh reader instead of rejecting an otherwise playable file.

Implementation head `0da69e30c7c0a2fee72ae414e640d292c4b9e3fc` first passed GitHub Actions run `35346407766`. After documentation was synchronized, exact-final-head `5a0c745ca41be608691ab60d82a1afecc5b9105b` passed run `35347079672`: Linux sanitizer/core/progress checks succeeded and Windows Server 2022 / MSVC x64 completed the native build, all CTest targets including the decoder matrix, no-audio GUI lifecycle smoke, staging and artifact upload. PR #7 then merged to `main` as `99ea0a22e929d62f3c1245ceeb24f9803ccc6616`.

This fixture matrix is stronger automated codec evidence, but it is not a claim that every real-world file is qualified. Multi-minute source material, mono variants, CBR/VBR MP3 diversity, damaged/truncated files, slow physical storage and reviewed listening remain separate gates.

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
- [x] Seek/refill hardening and starvation/refill smoothing pass exact-head Linux + Windows CI and are merged.
- [x] Decoder/codec-stress PR #7 passes exact-final-head Linux + Windows CI and is merged.
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
