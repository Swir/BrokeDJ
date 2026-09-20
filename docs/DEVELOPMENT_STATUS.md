# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `413c722ada39ac58568c2cef025cdbf5965aa97b`; PR #48 (`M1: verify staged Windows artifact integrity and launch`) is merged.
- Post-merge main workflow run `35519957972` for `413c722ada39ac58568c2cef025cdbf5965aa97b` completed successfully.
- Active development: PR #49 (`M3: add configurable smoothed crossfader curves`) from `feat/m3-crossfader-curves`.
- Functional code head before this checkpoint commit: `f8cda48c3733c67d0a3c3c7a96caafadd7bdd08f`.
- Code-head workflow run `35520606380` passed generated-progress/package-tool checks, Linux configure/build and the full ASan/UBSan CTest suite. The Windows x64 job had configured successfully and was still building when this checkpoint was written; this documentation commit creates a newer PR head, so only the new exact-final-head workflow may authorize merge.
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
   - Realtime contract stress now changes crossfader law while transport, FX, beat-loop and reverse/slip controls are also moving, and still requires zero callback heap allocation/deallocation.

3. **Native Windows-facing control and deterministic quality evidence**
   - Right-clicking the existing crossfader opens a native PL/EN menu for Constant Power, Linear and Fast Cut; no placeholder control or duplicate mixer state was introduced.
   - Quality tests cover law math, fail-safe inputs, production-engine center-level differences, finite automation and continuity across a live curve switch.
   - The feature does not close M3 and does not imply a transparent limiter, zero latency or complete professional gain staging.

## Gates still open

- PR #49 must remain unmerged until the exact-final-head workflow for the checkpointed branch is green.
- M1 still requires real Windows 11 clean-machine/manual resize/HiDPI/import/device-switching checks and physical four-output master 1/2 versus cue 3/4 verification.
- Representative user-owned/licensed music-domain BPM/key/grid evidence remains open.
- Production key-lock listening/latency, MIDI/controller mappings and concrete controller profiles remain unqualified.
- Mixer work still lacks complete trim/gain-staging, richer metering, booth/mic/ducking and qualified recording/limiter behavior.
- Physical storage/underrun behavior, multi-hour soak and public alpha/beta Release qualification remain open.

## Next largest step

Finish PR #49 first: require exact-final-head Linux sanitizers/full CTest plus Windows x64 build/full CTest/audio diagnostics/native no-audio GUI smoke/silent device probe/staging/downloaded-artifact smoke. After integration, continue the M3 mixer path with measured gain-staging/trim and metering work, while preserving the still-open M1 physical-hardware gates and M2 evidence/controller gates.
