# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`
- Latest merged package: PR `#11` — band-limited variable-rate conversion and interpolation-aware seek read-ahead
- Merge commit: `d15fbb8187e699f200bfb79666c637614cf128c7`
- Exact final PR head: `ae7e98c258a7c62e39f38ed3a5b06d5f1ac10032`
- Final PR validation run: `35361883586` — Linux ASan/UBSan core/progress checks and all five core-only CTest targets passed; Windows x64 configure/build/full CTest, native no-audio GUI smoke, staging and artifact upload passed
- Roadmap counter remains: **M0 complete; 1/10 equal-weight milestones = 10.0%**

## Completed in the current validation package

- Added a prepared 24-tap Blackman-windowed sinc kernel bank for effective source steps above one frame per output sample, reducing out-of-band energy before speed-up/downsampling while retaining Catmull-Rom for the non-downsampling path.
- Kept kernel construction outside the realtime callback; the callback uses bounded lookup and fixed-tap multiply-accumulate work, and the existing zero-heap realtime contract remains green.
- Added a deterministic 1.5x high-frequency render gate using an 8 kHz passband reference and a 19 kHz stopband source, with explicit RMS and stopband/passband limits rather than an unsupported listening-quality claim.
- Changed long-track seek refill ordering so the requested chunk and both immediate neighbours are available before deeper forward prefetch, supporting the wider interpolation footprint near chunk boundaries.
- Updated README, architecture, validation and changelog documentation without increasing roadmap completion.

## Validation state

- PR #11 final head `ae7e98c258a7c62e39f38ed3a5b06d5f1ac10032` passed exact-head run `35361883586` before merge.
- Linux sanitizer/core CI passed all five core-only CTest targets, including the new high-frequency render metric and callback heap-allocation contract.
- Windows x64 CI passed configure/build, the full CTest matrix including decoder fixtures, native no-audio GUI lifecycle smoke, staging and artifact upload.
- The finite windowed-sinc filter bank is an anti-aliasing improvement for pitch-changing rate conversion, **not** a key-lock/time-stretch engine or proof of perceptual transparency.
- No physical audio interface, Windows 11 clean-machine, controller, slow physical storage or reviewed listening validation was performed by this package.

## Remaining blockers / gates

1. Clean Windows 11 interactive launch, resize/import and actual audio-interface behavior still require manual verification.
2. Two-output and four-output physical-device routing, device loss/change and real cue isolation still require hardware evidence even though routing contracts are covered offline.
3. The automated codec matrix remains synthetic/generated; a broader real-world mono/stereo and CBR/VBR corpus, damaged/truncated variants and slow physical storage still need evidence.
4. Objective render metrics now include deterministic high-frequency anti-alias evidence, but they do not establish inaudibility, transparent limiting or production-quality key lock.
5. Physical device callback deadlines/underruns, soak testing, time-stretch/key-lock, beat analysis/grid and the rest of M2 remain open.

## Next highest-impact step

Benchmark the fixed-tap rate-conversion callback cost across representative source/output sample-rate combinations, extend deterministic spectral fixtures beyond the 1.5x case, then choose and prototype the production time-stretch/key-lock architecture without coupling analysis work to ordinary playback.
