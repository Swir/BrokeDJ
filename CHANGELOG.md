# Changelog

All notable BrokeDJ development changes are recorded here. BrokeDJ is still pre-alpha; this file does not imply that a public release exists.

## Unreleased

### Audio quality hardening

- Replaced the original linear variable-rate interpolation path with an allocation-free four-point Catmull-Rom interpolator.
- Added short de-click transitions around play/pause/seek discontinuities.
- Added per-sample smoothing for EQ, echo and drive control changes.
- Added a separate deterministic CTest quality suite for transport transitions and control automation.
- Migrated roadmap presentation to the SWIR SVG-only progress system and retired the previous standalone progress asset.
- Migrated README presentation to SWIR README PRO v2 while preserving BrokeDJ-specific branding and limitations.

### Validation status

- Previous `main` commit `c16bc227ebb33316b4c551750b7f12bbcd63f88d` passed the repository's Windows x64 development build, core tests and GUI lifecycle smoke workflow on 2026-09-18.
- The changes above require exact-head pull-request CI before merge and do not close M1 hardware/manual validation.
