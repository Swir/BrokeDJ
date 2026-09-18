# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current workstream

- Branch: `feat/audio-quality-hardening`
- Base: `main` at `c16bc227ebb33316b4c551750b7f12bbcd63f88d`
- Scope: variable-rate playback quality, transport/control de-clicking, deterministic quality tests, README PRO v2 and SVG-only progress migration
- Roadmap counter remains: **M0 complete; 1/10 equal-weight milestones = 10.0%**

## Verified before this branch

- `main` workflow run 35316793560 completed successfully on 2026-09-18.
- That workflow built the Windows x64 JUCE application, ran core tests, completed the no-audio GUI lifecycle smoke test, staged a development build and uploaded an artifact.
- M1 is still open because clean-machine interactive use, resize/import checks, device switching, real multi-output cue, disconnect recovery and soak testing are separate manual/hardware gates.

## Local/offline validation for this workstream

- C++20 compilation of the modified JUCE-independent core succeeded with GCC warning flags.
- New deterministic quality test executable passed 10 checks covering pause de-clicking, seek transition behavior and finite output under EQ/FX automation.
- GitHub exact-head pull-request CI is still required before merge.

## Remaining blockers / gates

1. Exact-head PR CI for Linux sanitizer/core tests and Windows x64 native build/smoke.
2. Clean Windows 11 interactive launch and resize/import verification.
3. Actual audio interface validation for two-output and four-output cue routing, device switching/disconnect and buffer/sample-rate changes.
4. Streaming/read-ahead architecture to remove the 256 MiB decoded-whole-track limitation.
5. Objective render fixtures and later listening tests before broader sound-quality claims.

## Next highest-impact step

After the current branch is green and merged, prioritize bounded streaming/read-ahead playback plus cancellation-safe long-file loading. Do not start VST3, stems or large effect inventory work ahead of this playback foundation.
