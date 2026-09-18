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

### Audio quality hardening

- Replaced the original linear variable-rate interpolation path with an allocation-free four-point Catmull-Rom interpolator.
- Added short de-click transitions around play/pause/seek discontinuities and whole-track loop wraparound.
- Added per-sample smoothing for playback-rate changes, headphone cue switching/level, EQ, echo and drive controls.
- Added deterministic quality checks for loop continuity, cue fade-out and rate-slew convergence.
- Migrated roadmap presentation to the SWIR SVG-only progress system and README presentation to SWIR README PRO v2.

### Validation status

- Merged-main checkpoint `cc5b43ebbd323d2119c54ff2fea8d8db9fab0199` passed Linux sanitizer/core checks and Windows x64 development build, core/quality tests and GUI lifecycle smoke in run `35331491062`.
- Bounded streaming PR #4 final head `9d842feecda23ea78904a35a9f4c073533784abc` passed exact-head run `35334147477`, including Linux sanitizers and Windows x64 build, CTest, GUI smoke, staging and artifact upload, then merged as `ac9a98610a166548c3ca0e18446fc95af99cd7c9`.
- The roadmap remains **1/10 = 10.0%** because this package does not close M1 hardware/manual validation or the broader M2 performance-deck scope.
