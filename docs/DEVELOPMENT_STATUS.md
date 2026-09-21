# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `334eac1168b67ae193dcc556a6ba4a2f22febef2`, the merge commit for PR #71 (`M4: ship self-contained M4 witness package`).
- PR #71 exact-final-head `1e85dd01e81857515da75d40294cda597fc13287` passed all required pull-request workflows before merge: M4 witness tool `35611432465`, Build and test `35611431863` and Native library scale smoke `35611431959`.
- Merged-main Native library scale smoke `35616709284` passed for `334eac1168b67ae193dcc556a6ba4a2f22febef2`; merged-main Build and test `35616709229` was still running at this checkpoint and must not be assumed green.
- Active development: draft PR #72, branch `feat/m1-self-contained-hardware-witness`. Functional head before this checkpoint documentation commit: `298f9c618c378283a1cba930f6709106c77a5a21`.
- PR #72 adds a privacy-minimized Windows 11 x64 M1 hardware witness recorder that binds evidence to the exact `BrokeDJ.exe` and exact `BrokeDJ-device-probe.json` by SHA-256, revalidates the silent probe safety contract, and requires a real four-output candidate before a complete M1 record can be accepted.
- The recorder never automates playback, device switching, cue routing or volume changes. Actual launch/resize/import/device-switch/four-output-cue checks remain explicit user-controlled booleans and therefore cannot be fabricated by CI.
- The M1 evidence schema excludes device names, track names/paths and source music. Validation rejects extra/free-form fields, type-spoofed values, app/probe fingerprint mismatches, unsafe probes and zero four-output candidates.
- Windows development packaging now stages `M1-HARDWARE-WITNESS.ps1` and `M1-HARDWARE-WITNESS.md` beside the exact staged EXE inside the existing package manifest/checksum contract; downloaded-artifact smoke requires both files.
- Dedicated `M1 hardware witness tool` CI parses the PowerShell recorder and exercises one accepted fixture plus incomplete, type-spoofed, privacy-unsafe, unexpected-field, executable/probe mismatch, unsafe-probe and zero-four-output negative fixtures. Semantic probe failures use canonical `BrokeDJ-device-probe.json` filenames in isolated directories so rejection is not accidentally satisfied by the filename guard alone.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). Neither M1 nor M4 is complete without the documented real Windows 11 manual evidence.
- GitHub Releases remains empty; no public BrokeDJ alpha/beta/stable release is qualified by this checkpoint.

## Integrated foundations on main

- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe backup/restore and native 5k-track/12k-history staged-EXE recovery.
- The M4 witness recorder/guide is now shipped beside the staged EXE and covered by package integrity checks. It remains a recorder/validator only; the real connected Windows 11 library/session workflow must still be performed.
- M1 already has Windows x64 CI compilation, no-audio GUI resize smoke, deterministic cue-routing tests and a silent schema-versioned device capability probe. None of those substitutes for real hardware switching or physical outputs 3/4 cue isolation.

## Gates still open

- PR #72 requires fresh exact-final-head Build and test, Native library scale smoke and M1 hardware witness-tool results after this checkpoint commit. Fix any regression before considering integration.
- M1 still requires the real Windows 11 clean launch/resize/import/playback/device-switch/four-output-cue witness on actual hardware.
- M4 still requires the real Windows 11 connected-library/session witness from the exact staged executable.
- M2 representative-music/key-lock/listening, M3 physical microphone/booth/recording/listening, controller profiles and long live soak remain separate release gates.

## Next largest step

Wait for the exact-final-head PR #72 checks, repair any regression, and keep the package on the development branch until green. Do not advance `docs/progress.json` from 1/10 until real acceptance evidence closes a milestone. Once the witness tooling is green, the remaining M1/M4 blockers are external/manual Windows 11 hardware and workflow evidence rather than missing recorder/package plumbing.
