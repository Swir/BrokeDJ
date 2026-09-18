# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`
- Latest merged package: PR `#12` — multi-rate resampler quality matrix and callback diagnostics
- Merge commit: `77ff9bca772b6b035f7c8ce76a1317789c485f26`
- Exact final PR head: `1747ed8d10f3f2151752bd1bf55963b9c17917b2`
- Final PR validation run: `35364173494` — Linux ASan/UBSan core/progress checks and all five core-only CTest targets passed; Windows x64 configure/build/full CTest, retained audio diagnostics, native no-audio GUI smoke, staging and artifact upload passed
- Roadmap counter remains: **M0 complete; 1/10 equal-weight milestones = 10.0%**

## Completed in the current validation package

- Expanded deterministic anti-alias coverage from one 48 kHz / 1.5x fixture to representative 44.1→48 kHz speed-up plus 96→48 kHz and 192→48 kHz source-rate conversion cases.
- Added per-path callback diagnostics for the lower-cost Catmull-Rom path and three fixed-tap windowed-sinc paths while preserving the zero-allocation/zero-deallocation callback contract.
- Retained verbose render and realtime metrics in `AUDIO-DIAGNOSTICS.txt` inside the Windows development artifact so quality/performance evidence can be inspected without treating a shared runner as hardware certification.
- Windows shared-runner diagnostics measured stopband/passband RMS ratios of `0.021501` (48 kHz 1.5x), `0.066392` (44.1→48 kHz 1.5x), `0.023641` (96→48 kHz) and `0.000041` (192→48 kHz) for the deterministic fixtures.
- Windows shared-runner callback diagnostics measured `357.714 ns/frame` for 44.1→48 kHz Catmull-Rom and roughly `887.588–933.840 ns/frame` for the tested fixed-tap sinc cases; all measured callback windows reported zero heap allocations and deallocations. These timings are diagnostic only.

## Validation state

- PR #12 final head `1747ed8d10f3f2151752bd1bf55963b9c17917b2` passed exact-head run `35364173494` before merge.
- Linux sanitizer/core CI passed generated-progress validation and all five core-only CTest targets.
- Windows x64 CI passed configure/build, full CTest including decoder fixtures, the explicit render/realtime diagnostic replay, native no-audio GUI lifecycle smoke, staging and artifact upload.
- The Windows artifact `BrokeDJ-0.1.0-development-windows-x64` includes `AUDIO-DIAGNOSTICS.txt` plus the development EXE and corresponding source archive; this remains a development artifact, not a public release.
- The finite windowed-sinc filter bank is an anti-aliasing improvement for pitch-changing rate conversion, **not** a key-lock/time-stretch engine or proof of perceptual transparency.
- No physical audio interface, Windows 11 clean-machine, controller, slow physical storage or reviewed listening validation was performed by this package.

## Remaining blockers / gates

1. Clean Windows 11 interactive launch, resize/import and actual audio-interface behavior still require manual verification.
2. Two-output and four-output physical-device routing, device loss/change and real cue isolation still require hardware evidence even though routing contracts are covered offline.
3. The automated codec matrix remains synthetic/generated; a broader real-world mono/stereo and CBR/VBR corpus, damaged/truncated variants and slow physical storage still need evidence.
4. Objective render metrics now cover multiple source/output-rate paths, but they do not establish inaudibility, transparent limiting or production-quality key lock.
5. Physical device callback deadlines/underruns, soak testing, time-stretch/key-lock, beat analysis/grid and the rest of M2 remain open.

## Next highest-impact step

Prototype a bounded production time-stretch/key-lock path behind a core-friendly interface, with deterministic ratio/pitch/latency/reset/seek fixtures and no mandatory dependency for ordinary playback. Candidate upstreams must be pinned and license-reviewed before integration; Signalsmith Stretch is an MIT-licensed C++ candidate and Rubber Band remains a GPL-2.0-or-later alternative. Keep the existing rate converter as the fallback until the new path passes objective and realtime-contract gates.
