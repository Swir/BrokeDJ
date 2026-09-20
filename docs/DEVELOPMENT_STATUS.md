# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `4c6c04548c358f032f576d791ce52d1c96cb1c88`; PR #49 (`M3: add configurable smoothed crossfader curves`) is merged.
- Active development: PR #50 (`M3: add input trim and richer mixer metering`) from `feat/m3-gain-staging-metering`.
- Functional head `8e31822ff38167c2ba58501be65080fadc6b36ee` passed exact-head workflow run `35525650716`: Linux sanitizer/full CTest, Windows x64 build/full CTest/audio diagnostics, native no-audio GUI lifecycle+resize smoke, silent device probe, staged package/source creation, uploaded-artifact checksum verification and downloaded staged-app smoke all succeeded.
- This documentation checkpoint is newer than that functional head and therefore must receive its own exact-final-head green workflow before PR #50 may merge.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). This package does not close M3.
- GitHub Releases remains empty; no public BrokeDJ Release is qualified by this checkpoint.

## Active M3 slice: gain staging and metering

1. **Separate input trim and channel fader**
   - Each deck now has a bounded `-24..+12 dB` input trim before EQ, effects and pre-fader headphone cue.
   - The existing post-FX channel gain remains a distinct fader stage instead of being repurposed as trim.
   - Non-finite trim input fails safe to unity; trim automation is smoothed inside the existing bounded callback path.

2. **Useful overload evidence**
   - Per-deck metering now exposes pre-fader peak, post-fader peak/RMS and a pre-fader overload flag.
   - Master metering now exposes both peak and RMS while preserving the existing pre-protection overload evidence.
   - The native PL/EN UI exposes TRIM and Fader separately, shows deck input peak/overload state, and reports master PK/RMS in dBFS.

3. **Realtime and regression coverage**
   - Deterministic quality tests cover dB conversion/clamping, +6 dB gain behavior, NaN fail-safe behavior, RMS/peak relationships, channel-overload visibility, cue semantics and separation from master overload.
   - Realtime stress automates trim/fader together with transport, effects, beat loop, reverse/slip and crossfader-curve changes while still requiring zero callback heap allocation/deallocation and finite meters/audio.
   - This is gain-staging infrastructure, not a claim of transparent limiting or physical-device qualification.

## Gates still open

- PR #50 must remain unmerged until the newest documentation-inclusive head has a fully green exact-head workflow.
- M1 still requires real Windows 11 clean-machine/manual resize/HiDPI/import/device-switching checks and physical four-output master 1/2 versus cue 3/4 verification.
- Representative user-owned/licensed music-domain BPM/key/grid evidence remains open.
- Production key-lock listening/latency, MIDI/controller mappings and concrete controller profiles remain unqualified.
- M3 still lacks configurable routing/booth, qualified EQ curves and limiter behavior, microphone/ducking, and dropout-aware set recording.
- Physical storage/underrun behavior, multi-hour soak and public alpha/beta Release qualification remain open.

## Next largest step

Finish PR #50 first and merge only after its exact-final-head Linux/Windows/package gate is green. Then continue the finish-first product path with a bounded dropout-aware set-recording pipeline and its recovery/file-integrity tests before spending time on optional mixer polish. Keep the still-open M1 physical-hardware and M2 evidence/controller gates explicit rather than treating CI as a substitute for them.
