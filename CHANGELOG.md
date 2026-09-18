# Changelog

All notable BrokeDJ development changes are recorded here. BrokeDJ is still pre-alpha; this file does not imply that a public release exists.

## Unreleased

### Audio quality hardening

- Replaced the original linear variable-rate interpolation path with an allocation-free four-point Catmull-Rom interpolator.
- Added short de-click transitions around play/pause/seek discontinuities and whole-track loop wraparound.
- Added per-sample smoothing for playback-rate changes, headphone cue switching/level, EQ, echo and drive controls.
- Added deterministic quality checks for loop continuity, cue fade-out and rate-slew convergence alongside the existing transport/control tests.
- Added a separate deterministic CTest quality suite for transport transitions and control automation.
- Migrated roadmap presentation to the SWIR SVG-only progress system and retired the previous standalone progress asset.
- Migrated README presentation to SWIR README PRO v2 while preserving BrokeDJ-specific branding and limitations.

### Validation status

- Previous merged audio-quality package `977ad6a16de0837666208d6b8b1a9459191a7f17` passed the repository's Linux sanitizer/core checks and Windows x64 development build, core/quality tests and GUI lifecycle smoke workflow on 2026-09-18.
- The new transport-continuity changes passed local JUCE-independent C++20 builds plus AddressSanitizer/UndefinedBehaviorSanitizer runs: **439 core checks and 18 quality checks**.
- The new changes still require exact-head pull-request CI before merge and do not close M1 hardware/manual validation.
