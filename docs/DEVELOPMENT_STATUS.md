# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`
- Active development branch: `feat/master-safety-protection`
- Pull request: `#10` — master/cue output safety protection and routing regression coverage
- Implementation head: `c74b0381381cc98ba2f1dd3cb92542b54da51d3c`
- Validation run started for that implementation head: `35355225489`; Linux ASan/UBSan core/progress checks passed, while the Windows x64 job was still running when this checkpoint was authored. The final PR head must pass its own complete CI before merge.
- Roadmap counter remains: **M0 complete; 1/10 equal-weight milestones = 10.0%**

## Completed in the current validation package

- Replaced the final hard master/cue output clamp with an allocation-free smooth safety curve that leaves signals at or below 0.90 linear unchanged and progressively approaches a 0.98 ceiling only in the top output region.
- Preserved pre-protection master peak and overload reporting, so the protection curve cannot hide bad gain staging behind a falsely clean meter.
- Added deterministic tests for below-knee transparency, bounded overloaded master output, finite protected output and summed cue-output protection.
- Strengthened cue routing regression coverage: private cue stays on logical outputs 3/4 in four-channel mode and is never folded into a two-channel master when dedicated cue outputs are unavailable.
- Closed stale documentation-only PR `#8` without force-updating it because newer merged PR `#9` and the current main checkpoint already superseded its evidence.

## Validation state

- PR #9 remains the latest merged and exact-head validated package on `main`: merge `90c7677dede1fc293fac31508f04ddaf8cbc9a13`, final PR head `98d52555ea207d3e1a22eb469d79981695820299`, run `35349954270`.
- PR #10 is intentionally not merged until the full Linux + Windows workflow is green for its final head.
- The new output stage is a bounded safety curve, **not** a transparent look-ahead limiter, true-peak limiter or mastering processor.
- No physical audio interface, Windows 11 clean-machine, controller, slow physical storage or reviewed listening validation was performed by this package.

## Remaining blockers / gates

1. Clean Windows 11 interactive launch, resize/import and actual audio-interface behavior still require manual verification.
2. Two-output and four-output physical-device routing, device loss/change and real cue isolation still require hardware evidence even though routing contracts are covered offline.
3. The automated codec matrix remains synthetic/generated; a broader real-world mono/stereo and CBR/VBR corpus, damaged/truncated variants and slow physical storage still need evidence.
4. Objective render metrics protect current rate conversion and transport transitions, but they do not prove full-spectrum anti-alias quality, inaudibility or transparent limiting.
5. Physical device callback deadlines/underruns, soak testing, time-stretch/key-lock, beat analysis/grid and the rest of M2 remain open.

## Next highest-impact step

Finish PR #10 only after exact-final-head Linux + Windows CI is green, then extend objective resampler coverage toward high-frequency/alias behavior and use that evidence to choose the next production-quality tempo/key-lock path without weakening the separate Windows 11/audio-hardware gates.
