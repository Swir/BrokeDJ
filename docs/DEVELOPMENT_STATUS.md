# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`
- Active development branch: `feat/render-metrics-rt-contract`
- Pull request: `#9` — objective offline render metrics plus real-time callback heap contract
- Latest implementation head validated before documentation-only checkpoint updates: `9ed8f5ae828602c3897e56ef29018ffed926f924`
- Validation run: `35349145885` — Linux ASan/UBSan + all five core-only CTest targets passed; Windows x64 configure/build/full CTest including decoder fixtures/native no-audio GUI smoke/staging passed
- Roadmap counter remains: **M0 complete; 1/10 equal-weight milestones = 10.0%**

## Completed in the current validation package

- Added deterministic offline render metrics for 0.75x, 1.0x and 1.25x pitch-changing rate conversion, including frequency error, residual RMS ratio, DC, RMS and peak checks.
- Added numeric maximum adjacent-sample-delta gates for seek, pause and whole-track loop transitions instead of relying only on qualitative assertions.
- Added a dedicated callback heap contract test that stresses in-memory and streamed playback plus repeated seek/rate/EQ/FX/cue/crossfader changes while requiring zero heap allocations and deallocations during the measured callback window.
- Added diagnostic callback elapsed-time reporting without using shared CI runner timing as a release/performance threshold.
- Kept production DSP unchanged in this package; the work strengthens regression evidence rather than inflating the roadmap.

## Validation state

- The first render-metrics run failed because its step fixture was only 4096 frames and crossed the polarity boundary before the intended steady-state measurement. The fixture was corrected to keep the measurement window valid; audio thresholds were not weakened.
- Implementation head `9ed8f5ae828602c3897e56ef29018ffed926f924` passed run `35349145885` on both required jobs.
- Documentation-only checkpoint updates after that implementation head still require a fresh exact-final-head run before PR #9 may merge.
- No physical audio interface, Windows 11 clean-machine, controller, slow physical storage or reviewed listening validation was performed by this package.

## Remaining blockers / gates

1. Clean Windows 11 interactive launch, resize/import and actual audio-interface behavior still require manual verification.
2. The automated codec matrix remains synthetic/generated; a broader real-world mono/stereo and CBR/VBR corpus, damaged/truncated variants and slow physical storage still need evidence.
3. Objective render metrics now protect current rate conversion and transport transitions, but they do not prove transparency, anti-aliasing quality across the full spectrum or inaudibility.
4. The callback contract now enforces zero observed heap allocation/deallocation in the tested stress path, but physical device callback deadlines and underruns still require hardware evidence.
5. Time-stretch/key-lock, beat analysis/grid and the rest of M2 remain open.

## Next highest-impact step

Finish PR #9 on an exact-final-head green run, then extend objective resampler coverage toward high-frequency/alias behavior and use that evidence to choose the next production-quality tempo/key-lock path without weakening the separate Windows 11/audio-hardware gate.
