# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`
- Latest merged package: PR `#10` — master/cue output safety protection and routing regression coverage
- Merge commit: `a207b89e573c6068285e24bf9748c9ecefcaaf69`
- Exact final PR head: `f82a0dab7dde1298690dbe326049d21190c0be90`
- Final PR validation run: `35355739490` — Linux ASan/UBSan core/progress checks passed; Windows x64 configure/build/full CTest, native no-audio GUI smoke, staging and artifact upload passed
- Roadmap counter remains: **M0 complete; 1/10 equal-weight milestones = 10.0%**

## Completed in the current validation package

- Replaced the final hard master/cue output clamp with an allocation-free smooth safety curve that leaves signals at or below 0.90 linear unchanged and progressively approaches a 0.98 ceiling only in the top output region.
- Preserved pre-protection master peak and overload reporting, so the protection curve cannot hide bad gain staging behind a falsely clean meter.
- Added deterministic tests for below-knee transparency, bounded overloaded master output, finite protected output and summed cue-output protection.
- Strengthened cue routing regression coverage: private cue stays on logical outputs 3/4 in four-channel mode and is never folded into a two-channel master when dedicated cue outputs are unavailable.
- Closed stale documentation-only PR `#8` without force-updating it because newer merged work had already superseded its evidence.

## Validation state

- PR #10 final head `f82a0dab7dde1298690dbe326049d21190c0be90` passed exact-head run `35355739490` on both required jobs before merge.
- The new output stage is a bounded safety curve, **not** a transparent look-ahead limiter, true-peak limiter or mastering processor.
- No physical audio interface, Windows 11 clean-machine, controller, slow physical storage or reviewed listening validation was performed by this package.

## Remaining blockers / gates

1. Clean Windows 11 interactive launch, resize/import and actual audio-interface behavior still require manual verification.
2. Two-output and four-output physical-device routing, device loss/change and real cue isolation still require hardware evidence even though routing contracts are covered offline.
3. The automated codec matrix remains synthetic/generated; a broader real-world mono/stereo and CBR/VBR corpus, damaged/truncated variants and slow physical storage still need evidence.
4. Objective render metrics protect current rate conversion and transport transitions, but they do not prove full-spectrum anti-alias quality, inaudibility or transparent limiting.
5. Physical device callback deadlines/underruns, soak testing, time-stretch/key-lock, beat analysis/grid and the rest of M2 remain open.

## Next highest-impact step

Extend objective resampler coverage toward high-frequency/alias behavior and use that evidence to choose the next production-quality tempo/key-lock path, while keeping clean Windows 11 and physical audio-hardware validation as separate release gates.
