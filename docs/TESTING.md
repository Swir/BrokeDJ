# Validation record

## Verified merged baseline: 2026-09-18

Transport-continuity PR #3 was merged as `6075283f3029a78eb587fb09eda98b7246b444d5`; merged-main checkpoint `cc5b43ebbd323d2119c54ff2fea8d8db9fab0199` passed GitHub Actions run `35331491062`.

That baseline includes Linux ASan/UBSan core checks, generated SVG synchronization, Windows Server 2022 / MSVC x64 configure + Release build, core/quality CTest, native no-audio GUI lifecycle smoke and development artifact staging. These checks do not certify physical audio hardware or live reliability.

## Bounded streaming/read-ahead package

PR #4 added a fixed-size stream cache to the JUCE-independent core and a background JUCE read-ahead source for larger local tracks. Small supported tracks retain the in-memory path. The streaming path removes the former fixed 256 MiB decoded-whole-track ceiling without moving file I/O or decoding into the audio callback.

The final PR head `9d842feecda23ea78904a35a9f4c073533784abc` passed GitHub Actions run `35334147477`: Linux sanitizer/core tests and SVG checks succeeded; Windows Server 2022 / MSVC x64 configure/build, all CTest targets including streaming cache, native no-audio GUI smoke, staging and artifact upload also succeeded. PR #4 was then merged to `main` as `ac9a98610a166548c3ca0e18446fc95af99cd7c9`.

Deterministic streaming coverage verifies:

- a cache miss produces bounded silence and requests the missing source region;
- a published chunk becomes visible and preserves stereo sample data;
- stream-backed clips satisfy core validation and can be submitted to a deck;
- cached streamed audio renders finite, non-zero output through the engine;
- a seek into an uncached region updates the read-ahead request instead of doing callback I/O.

## Seek/refill and starvation hardening

PR #5 added deterministic virtual long-track seek/loop/refill coverage and prioritized the exact requested cache region before forward read-ahead. It passed exact-final-head Linux + Windows validation and merged as `7cced0d18fe7d192fa983b482d2637b2204fe0f5`.

PR #6 added lock-free starvation/refill episode counters, readiness-aware interpolation tap handling and short fade transitions into/out of cache starvation. Final head `915441c73cc6bab7c58579a8343952ca9e1a9bf1` passed run `35338665653` and merged as `58ecdb3b85b8ff395d115097776d5946bf457936`.

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

The first Windows PR runs exposed an MP3 forced-streaming regression that Linux core/sanitizer checks could not reveal. The decoder now treats waveform generation and streaming playback as independent reader lifetimes; if a compressed reader rejects the sparse non-monotonic preview pattern, waveform construction falls back to a bounded-memory sequential pass on a fresh reader instead of rejecting an otherwise playable file.

PR #7 passed its exact-final-head gate and merged to `main` as `99ea0a22e929d62f3c1245ceeb24f9803ccc6616`.

This fixture matrix is stronger automated codec evidence, but it is not a claim that every real-world file is qualified. Multi-minute source material, mono variants, CBR/VBR MP3 diversity, damaged/truncated files, slow physical storage and reviewed listening remain separate gates.

## Objective render and real-time contract package

PR #9 adds two JUCE-independent CTest targets without changing production DSP:

- `render_metrics` renders deterministic tones at 0.75x, 1.0x and 1.25x and checks measured frequency error, fundamental residual RMS ratio, DC, RMS and output peak. It also measures maximum adjacent-sample deltas across seek, pause and whole-track loop transitions.
- `realtime_contract` instruments heap allocation in its own test executable and drives 1,200 callback blocks with in-memory and stream-backed decks while repeatedly changing seek, rate, EQ, echo, drive, cue and crossfader controls. Heap allocations and deallocations must both remain zero during the measured callback window. It reports elapsed callback cost as diagnostic data only; shared-runner timing is not a release threshold.

The first render-metrics run correctly failed because the step fixture was too short and had already crossed its polarity boundary before the measurement window. The fixture was corrected without weakening the audio thresholds.

Final PR #9 head `98d52555ea207d3e1a22eb469d79981695820299` passed GitHub Actions run `35349954270`: Linux ASan/UBSan build plus all five core-only CTest targets succeeded; Windows Server 2022 / MSVC x64 configure/build, the full CTest matrix including decoder fixtures, native no-audio GUI lifecycle smoke, staging and artifact upload also succeeded. PR #9 then merged as `90c7677dede1fc293fac31508f04ddaf8cbc9a13`.

These objective checks are regression gates for the current implementation. They do **not** establish inaudibility, transparent time-stretch, true-peak limiting, device latency or professional live readiness.

## Output safety and cue-routing package

PR #10 replaces the final hard master/cue sample clamp with a smooth allocation-free safety curve. Samples at or below 0.90 linear pass unchanged; only the top region is progressively compressed toward the 0.98 ceiling. The master meter and overload flag still observe the pre-protection signal, so the safety stage cannot hide excessive gain.

`brokedj_quality_tests` now verifies:

- below-knee output remains effectively identical to the pre-protection master meter;
- pre-protection overload remains visible while the rendered master stays finite and within the 0.98 ceiling;
- private cue remains absent from master when channel gain is down;
- four-channel mode keeps independent stereo cue on outputs 3/4;
- rendering the same state to a two-channel device never folds private cue into master;
- summed cue output is bounded by the same smooth safety curve.

Final PR #10 head `f82a0dab7dde1298690dbe326049d21190c0be90` passed exact-head GitHub Actions run `35355739490`: Linux ASan/UBSan core/progress checks succeeded; Windows x64 configure/build, full CTest, native no-audio GUI smoke, staging and artifact upload also succeeded. PR #10 merged to `main` as `a207b89e573c6068285e24bf9748c9ecefcaaf69`.

This is deterministic routing/DSP evidence only. The protection curve is **not** a transparent look-ahead limiter or true-peak limiter, and offline output routing tests do not replace physical two-output/four-output interface validation or reviewed listening.

## Band-limited rate-conversion package

PR #11 added a prepared 24-tap Blackman-windowed sinc lookup bank for effective downsampling/speed-up while retaining Catmull-Rom interpolation when no anti-alias low-pass is required. Kernel generation happens in `prepare()` while audio is stopped; the callback performs bounded table lookup and multiply-accumulate work only.

`render_metrics` added a deterministic high-frequency anti-alias gate. At 1.5x playback it renders an 8 kHz source as a passband reference and a 19 kHz source as an out-of-band case that would fold into the audible band without pre-decimation filtering. The gate requires the 8 kHz reference to retain useful RMS level, the 19 kHz stopband RMS to remain below 0.02 and the stopband/passband RMS ratio to remain below 0.10. These thresholds are implementation regression guards, not psychoacoustic transparency claims.

The large-track worker now prioritizes the requested chunk and both immediate neighbours before deeper forward read-ahead, because the wider interpolation kernel can need samples on either side of a seek cursor. Actual physical-storage refill latency and hardware underruns remain unqualified.

Final PR #11 head `ae7e98c258a7c62e39f38ed3a5b06d5f1ac10032` passed exact-head GitHub Actions run `35361883586`: Linux ASan/UBSan core/progress checks and all five core-only CTest targets succeeded; Windows x64 configure/build, full CTest including decoder fixtures, native no-audio GUI smoke, staging and artifact upload also succeeded. PR #11 merged as `d15fbb8187e699f200bfb79666c637614cf128c7`.

## Multi-rate resampler matrix and callback diagnostics package

PR #12 expanded the objective spectral baseline across representative source/output-rate paths and made callback-cost evidence persist with the Windows development artifact. Production DSP topology is unchanged from PR #11.

`render_metrics` now gates four deterministic anti-alias cases: 48 kHz at 1.5x, 44.1→48 kHz at 1.5x, 96→48 kHz at 1.0x and 192→48 kHz at 1.0x. On the exact-head Windows run the observed stopband/passband RMS ratios were `0.021501`, `0.066392`, `0.023641` and `0.000041` respectively; every case stayed inside its explicit regression threshold.

`realtime_contract` now separately exercises a 44.1→48 kHz Catmull-Rom path plus 48 kHz 1.5x, 96→48 kHz and 192→48 kHz fixed-tap sinc paths. All measured windows retained zero heap allocations/deallocations. The Windows shared runner reported `357.714 ns/frame` for the Catmull-Rom case and `933.840`, `893.594` and `887.588 ns/frame` for the three sinc cases. These numbers are **diagnostic only**: they are not latency, underrun or supported-hardware guarantees and are not used as absolute CI performance thresholds.

The Windows workflow repeats the two diagnostic targets verbosely after the full CTest pass and stores their output as `AUDIO-DIAGNOSTICS.txt` in the development artifact. Final PR #12 head `1747ed8d10f3f2151752bd1bf55963b9c17917b2` passed exact-head run `35364173494`: Linux ASan/UBSan + all five core-only CTest targets succeeded; Windows x64 configure/build/full CTest, diagnostic replay, native no-audio GUI lifecycle smoke, staging and artifact upload succeeded. PR #12 merged as `77ff9bca772b6b035f7c8ce76a1317789c485f26`.

## Audio quality hardening coverage

`brokedj_quality_tests` covers stable playback, pause/seek transitions, loop-wrap continuity, cue switching continuity, control automation, master/cue output protection and cue routing isolation. `brokedj_render_metrics_tests` adds reproducible numeric transition and pitch-changing rate-conversion measurements, including multi-rate high-frequency passband/stopband evidence for downsampling. `brokedj_realtime_contract_tests` adds zero-heap callback coverage plus path-specific diagnostic cost measurements. These tests support implementation hardening; reviewed listening on representative material and devices remains required before stronger sound-quality claims.

## Existing core coverage

Empty silence; invalid clip/deck/device-rate rejection; stereo playback; playhead/rate/seek behavior; crossfader endpoints; cue isolation on two/four outputs; EOF; whole-track looping; output protection/finite values; three-band kill; an echo impulse; stopped replacement; retirement backpressure; concurrent publication and shutdown.

## What automated checks do not certify

Automated CI does not by itself certify Windows 11 clean-machine usability, physical audio hardware, device switching, real four-output cue isolation, controller support, latency, listening quality, zero audible clicks/dropouts, ASIO support, or multi-hour live reliability. The hybrid rate converter remains pitch-changing resampling, not key lock/time stretch; its finite windowed-sinc anti-alias matrix does not prove ideal reconstruction or inaudibility on arbitrary music. Shared-runner callback-cost measurements are diagnostics, not physical-device deadline/underrun evidence. The smooth output safety curve is not a transparent/look-ahead or true-peak limiter.

## Native acceptance checklist

- [x] Windows x64 build and native no-audio smoke mode pass on the merged baseline.
- [x] SVG progress synchronization and no-legacy-meter check pass on the merged baseline.
- [x] Bounded streaming/read-ahead PR passes exact-head Linux + Windows CI and is merged.
- [x] Seek/refill hardening and starvation/refill smoothing pass exact-head Linux + Windows CI and are merged.
- [x] Decoder/codec-stress PR #7 passes exact-final-head Linux + Windows CI and is merged.
- [x] Objective offline render metrics and callback heap-allocation contract pass exact-head Linux sanitizer and Windows x64 CI for PR #9.
- [x] Master/cue safety protection and routing regressions pass exact-final-head Linux + Windows CI and merge through PR #10.
- [x] Band-limited rate-conversion package passes exact-head Linux sanitizer and Windows x64 CI and merges through PR #11.
- [x] Multi-rate spectral matrix, per-path zero-heap callback diagnostics and retained Windows artifact evidence pass exact-head CI and merge through PR #12.
- [ ] Clean Windows 11 machine launches and logs startup correctly.
- [ ] Broader real-world mono/stereo WAV, FLAC, OGG, AIFF and CBR/VBR MP3 corpus decodes/streams as expected.
- [ ] Invalid, truncated and Unicode-path files fail clearly without losing working audio across the broader corpus.
- [ ] Large/long files stay memory-bounded under repeated seek/loop/load stress on real supported codecs/storage.
- [ ] Minimum and large window sizes keep every control reachable.
- [ ] Two-output and four-output physical devices have correct master/cue isolation.
- [ ] Device change/disconnect and closing during decode/streaming recover safely.
- [ ] Sample-rate/buffer changes are exercised without invalid output or transport corruption.
- [ ] Long-running simultaneous playback and repeated loading pass an agreed soak test.
- [ ] Transport/seek/loop/cue/cache transitions receive reviewed listening checks on representative fixtures and hardware.

Use original/generated or appropriately licensed audio fixtures only. Report commit, OS, device/driver, sample rate, buffer size, reproduction steps and a reviewed/redacted log. Do not mark a release gate complete based only on a scheduled run, screenshot or compile result.
