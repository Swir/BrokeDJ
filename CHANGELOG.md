# Changelog

All notable BrokeDJ development changes are recorded here. BrokeDJ is still pre-alpha; this file does not imply that a public release exists.

## Unreleased

### Long-track playback foundation

- Added a fixed-capacity lock-free stream cache to the JUCE-independent core.
- Added a background JUCE read-ahead source for larger supported local tracks while retaining the low-overhead in-memory path for small files.
- Removed the previous fixed 256 MiB decoded-whole-track rejection from the large-track streaming path.
- Added bounded sparse waveform preview generation so long tracks do not require a full decoded waveform allocation.
- Added deterministic cache miss/publication/stereo/render/seek-request tests as a separate CTest target.
- Kept file I/O and decoding outside the audio callback; a missing cache region returns bounded silence and requests background refill rather than blocking.
- Fixed streaming priming so decoder read failures reject the import instead of accepting an incompletely prepared source.
- Added a non-audio cache-readiness diagnostic snapshot for the requested region and bounded forward coverage without mislabelling it as a hardware underrun counter.
- Prioritized the exact requested chunk after a seek before forward read-ahead, reducing avoidable refill latency.
- Prevented the read-ahead worker from treating chunks beyond EOF as completed work, avoiding a potential busy loop near the end of long tracks.
- Expanded deterministic streaming stress coverage with a virtual 90-minute stream, repeated distant seeks, whole-track loop wrap and starvation/refill snapshots without allocating a full track.
- Added lock-free cache read-miss totals plus starvation/refill episode counters and last-miss position so repeated misses can be diagnosed without logging or allocating on the audio callback.
- Added readiness-aware streamed interpolation: an incomplete interpolation tap set is treated as unavailable instead of blending partially cached samples.
- Added short fade-out/fade-in transitions when a streamed deck enters or exits cache starvation, reducing hard discontinuities around refill onset while keeping decoder work off the audio thread.
- Added deterministic tests proving continuous starvation is counted as one episode and refill recovery ramps from silence instead of jumping directly to the recovered sample.
- Split the JUCE decoder/read-ahead adapter into an independently testable target and added generated WAV/AIFF/FLAC/OGG plus original synthetic MP3 fixtures for in-memory and forced-streaming paths.
- Added Unicode-path, invalid-file, cancellation, distant-seek and controlled slow-reader tests using the real decoder adapter.
- Added last-request-wins preemption between read-ahead chunks so a new seek abandons stale forward work as soon as the in-flight chunk completes.
- Sanitized non-finite decoded samples before publishing them to the stream cache.
- Added a bounded-memory sequential waveform fallback for compressed readers that reject sparse non-monotonic preview seeks, while giving streaming playback a fresh decoder instance. This fixed the Windows MP3 forced-streaming regression exposed by the new test matrix.
- Changed post-seek read-ahead ordering to fill the requested chunk and its immediate previous/next neighbours before deeper forward prefetch, supporting wider interpolation kernels without making the callback wait for decoder work.

### Audio quality hardening

- Replaced the original linear variable-rate interpolation path with an allocation-free four-point Catmull-Rom interpolator.
- Added short de-click transitions around play/pause/seek discontinuities and whole-track loop wraparound.
- Added per-sample smoothing for playback-rate changes, headphone cue switching/level, EQ, echo and drive controls.
- Added deterministic quality checks for loop continuity, cue fade-out and rate-slew convergence.
- Added objective offline render metrics for pitch-changing rate conversion: frequency error, residual RMS ratio, DC, RMS and peak are checked at 0.75x, 1.0x and 1.25x.
- Added deterministic maximum adjacent-sample-delta gates for seek, pause and whole-track loop transitions.
- Added a callback heap contract stress test covering in-memory and streamed decks plus seek/rate/EQ/FX/cue/crossfader automation; allocation and deallocation counts must remain zero during the measured callback window.
- Added diagnostic callback elapsed-time reporting without turning shared CI runner timing into a release/performance threshold.
- Replaced the final hard master/cue sample clamp with an allocation-free smooth safety curve that leaves the normal region unchanged and progressively approaches the 0.98 output ceiling only near overload; this remains a safety stage, not a transparent/look-ahead limiter.
- Added deterministic master/cue protection and routing tests covering below-knee transparency, preserved pre-protection overload evidence, bounded finite output, independent outputs 3/4 cue, no stereo cue fold-down and summed cue protection.
- Added a prepared 24-tap Blackman-windowed sinc filter bank for effective downsampling/speed-up while retaining the lower-cost Catmull-Rom path where no anti-alias low-pass is required; kernel generation remains outside the audio callback.
- Added a deterministic 1.5x high-frequency regression gate that compares an 8 kHz passband reference with a 19 kHz out-of-band source and limits both absolute stopband RMS and its ratio to the passband.
- Expanded deterministic anti-alias coverage across 44.1→48 kHz speed-up, 96→48 kHz and 192→48 kHz source-rate conversion paths, retaining the original 48 kHz / 1.5x case.
- Added representative callback-cost diagnostics for Catmull-Rom and fixed-tap sinc conversion paths while continuing to require zero heap allocations/deallocations in the measured callback windows.
- Windows development artifacts now retain `AUDIO-DIAGNOSTICS.txt` with verbose render and realtime metrics so CI evidence can be reviewed without treating shared-runner timing as hardware certification.
- Migrated roadmap presentation to the SWIR SVG-only progress system and README presentation to SWIR README PRO v2.

### Validation status

- Merged-main checkpoint `cc5b43ebbd323d2119c54ff2fea8d8db9fab0199` passed Linux sanitizer/core checks and Windows x64 development build, core/quality tests and GUI lifecycle smoke in run `35331491062`.
- Bounded streaming PR #4 final head `9d842feecda23ea78904a35a9f4c073533784abc` passed exact-head run `35334147477`, including Linux sanitizers and Windows x64 build, CTest, GUI smoke, staging and artifact upload, then merged as `ac9a98610a166548c3ca0e18446fc95af99cd7c9`.
- Stream seek/refill hardening PR #5 passed exact-final-head Linux sanitizer and Windows x64 build/test/GUI-smoke validation and merged to `7cced0d18fe7d192fa983b482d2637b2204fe0f5`.
- Starvation-history/refill-smoothing PR #6 final head `915441c73cc6bab7c58579a8343952ca9e1a9bf1` passed run `35338665653`: Linux sanitizer/core/SVG checks and Windows x64 configure/build/CTest/native no-audio GUI-smoke/staging succeeded; it merged as `58ecdb3b85b8ff395d115097776d5946bf457936`.
- Decoder/codec-stress PR #7 passed exact-final-head validation and merged to `main` as `99ea0a22e929d62f3c1245ceeb24f9803ccc6616` after the Windows-only MP3 forced-streaming regression was fixed.
- Objective-render/realtime-contract PR #9 final head `98d52555ea207d3e1a22eb469d79981695820299` passed exact-head run `35349954270`: Linux ASan/UBSan + all five core-only CTest targets succeeded, and Windows x64 configure/build, the full CTest matrix including decoder fixtures, native no-audio GUI smoke, staging and artifact upload succeeded; it merged as `90c7677dede1fc293fac31508f04ddaf8cbc9a13`.
- Output-safety/cue-routing PR #10 final head `f82a0dab7dde1298690dbe326049d21190c0be90` passed exact-head run `35355739490`: Linux ASan/UBSan core/progress checks succeeded, and Windows x64 configure/build, full CTest, native no-audio GUI smoke, staging and artifact upload succeeded; it merged as `a207b89e573c6068285e24bf9748c9ecefcaaf69`.
- Band-limited rate-conversion PR #11 final head `ae7e98c258a7c62e39f38ed3a5b06d5f1ac10032` passed exact-head run `35361883586`: Linux ASan/UBSan core/progress checks and all five core-only CTest targets succeeded; Windows x64 configure/build, full CTest including decoder fixtures, native no-audio GUI smoke, staging and artifact upload also succeeded. PR #11 merged as `d15fbb8187e699f200bfb79666c637614cf128c7`.
- Resampler-matrix/callback-diagnostics PR #12 final head `1747ed8d10f3f2151752bd1bf55963b9c17917b2` passed exact-head run `35364173494`: Linux ASan/UBSan and all five core-only CTest targets succeeded; Windows x64 configure/build/full CTest, explicit render/realtime diagnostic replay, native no-audio GUI smoke, staging and artifact upload succeeded. PR #12 merged as `77ff9bca772b6b035f7c8ce76a1317789c485f26`.
- The roadmap remains **1/10 = 10.0%** because these hardening packages do not close M1 hardware/manual validation or the broader M2 performance-deck scope.
