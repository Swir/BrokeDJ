# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `413c722ada39ac58568c2cef025cdbf5965aa97b`; PR #48 (`M1: verify staged Windows artifact integrity and launch`) is merged.
- Post-merge main workflow run `35519957972` for `413c722ada39ac58568c2cef025cdbf5965aa97b` completed successfully.
- Active development: PR #49 (`M3: add configurable smoothed crossfader curves`) from `feat/m3-crossfader-curves`.
- Previous checkpoint head `8a786e77fc67123ee912000c263f108dc8b37bb7` passed the Linux sanitizer/full-CTest job but failed the Windows x64 build in run `35520885684`.
- The Windows failure was a real MSVC portability regression: the constructor iterated `{&crossfader, &master, &headphone}` with `auto*`, but `crossfader` had become `CrossfaderSlider*` while the other two remained `juce::Slider*`, so MSVC could not deduce one initializer-list pointer type.
- Repair code head `64eda004806d12849623699fd2d9c2a13b05c097` makes the three top-level mixer sliders one concrete slider type while keeping the curve-menu path enabled only on the actual crossfader. Ordinary master/headphone slider interaction still delegates directly to JUCE.
- Exact repair-head workflow run `35522114779` is the current code gate. At this checkpoint its Linux and Windows jobs are still running, so PR #49 remains unmerged. This status commit creates a newer documentation-only head and therefore also requires its own exact-final-head green run before merge.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA).
- GitHub Releases remains empty; no public BrokeDJ Release exists or is qualified by this checkpoint.

## Active M3 slice: configurable crossfader laws

1. **Three real mixer laws, core-first**
   - JUCE-independent `CrossfaderCurve` supports Constant Power (default), Linear and Fast Cut.
   - Invalid raw mode values fail safe to Constant Power and non-finite positions resolve to a finite center state.
   - Fast Cut uses a bounded continuous cubic transition instead of a discontinuous hard switch.

2. **Realtime-safe live switching**
   - The engine keeps its existing bounded crossfader-position smoothing and additionally smooths the resulting left/right gain pair, so a live curve-mode change does not create an unsmoothed gain discontinuity.
   - Curve selection is a lock-free atomic control; the callback performs no I/O, allocation, deallocation, locking or unbounded work for this feature.
   - Realtime contract stress changes crossfader law while transport, FX, beat-loop and reverse/slip controls are also moving, and still requires zero callback heap allocation/deallocation.

3. **Native Windows-facing control and compiler portability**
   - Right-clicking the actual crossfader opens a native PL/EN menu for Constant Power, Linear and Fast Cut; master/headphone sliders do not expose that menu.
   - Quality tests cover law math, fail-safe inputs, production-engine center-level differences, finite automation and continuity across a live curve switch.
   - The Windows-only compile regression found by exact-head CI was fixed before any further feature expansion; the repair is still awaiting the full exact-head gate.
   - The feature does not close M3 and does not imply a transparent limiter, zero latency or complete professional gain staging.

## Gates still open

- PR #49 must remain unmerged until the exact-final-head workflow for the newest branch head is green.
- M1 still requires real Windows 11 clean-machine/manual resize/HiDPI/import/device-switching checks and physical four-output master 1/2 versus cue 3/4 verification.
- Representative user-owned/licensed music-domain BPM/key/grid evidence remains open.
- Production key-lock listening/latency, MIDI/controller mappings and concrete controller profiles remain unqualified.
- Mixer work still lacks complete trim/gain-staging, richer metering, booth/mic/ducking and qualified recording/limiter behavior.
- Physical storage/underrun behavior, multi-hour soak and public alpha/beta Release qualification remain open.

## Next largest step

Finish PR #49 first. Do not widen mixer scope until the repaired exact-final-head passes Linux sanitizers/full CTest plus Windows x64 build/full CTest/audio diagnostics/native no-audio GUI smoke/silent device probe/staging/downloaded-artifact smoke. After normal integration, continue the finish-first product path with measured trim/gain staging and richer metering while preserving the still-open M1 physical-hardware gates and M2 evidence/controller gates.
