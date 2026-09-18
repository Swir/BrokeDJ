<!-- SWIR-ROADMAP-STANDARD:v1 -->
# BrokeDJ roadmap

**Professional destination. Evidence-based milestones. No deadline or feature-parity promises.**

<img width="100%" src="assets/readme/progress-mini.svg" alt="BrokeDJ roadmap milestone progress" />

**Verified roadmap scope:** 1/10 equal-weight milestones complete · **10.0%** · source: [`docs/progress.json`](docs/progress.json). This is not a live-readiness or sound-quality score.

The initial milestone is complete only for the repository, testable core and documented architecture. App compilation, usability, hardware and production-readiness gates are separate. A finished subtask does not close a milestone automatically.

| ID | Milestone | Acceptance criteria | Status |
|---|---|---|---|
| M0 | Repository and audio foundation | C++20 core, reproducible test command, concurrent clip handoff tests, source layout, build definitions, license/branding and explicit limitations | Complete; local core validation recorded |
| M1 | Native development build | Windows x64 build passes; clean-machine launch; resize and import checks; device switching and four-output cue verified | Windows CI build + headless GUI smoke pass; manual clean-machine/hardware validation pending |
| M2 | Performance decks | Validated beat/tempo/key analysis, editable grids including tempo changes, key lock, hotcues, beat loops, slip/reverse/scratch and de-clicked transitions | Transport/seek de-clicking and improved rate interpolation implemented in development; milestone still open |
| M3 | Professional mixer and recording | Configurable routing, gain staging, EQ curves, fader laws, limiter evaluation, microphone/ducking and dropout-aware set recording | Basic source only; full milestone open |
| M4 | Music library and sessions | SQLite migrations, search/tags/playlists/history, duplicate and moved-file handling, waveform/analysis cache, session persistence and tested backup restore | Planned |
| M5 | Effects and VST3 | Distinct effect inventory, chains/sends, presets, XY/macros, automation smoothing, scanner isolation, license review and a tested compatibility matrix | Two built-in effects; parameter smoothing added; milestone open |
| M6 | Sampler and controllers | User sample/loop banks, live capture, quantized triggers, MIDI Learn and verified controller mappings | Planned |
| M7 | Stems and creative assistance | Separate stem decks, tested offline separation, optional live processing with published hardware limits, user-controllable set suggestions | Planned |
| M8 | Advanced interoperability | Documented app synchronization, streaming routes and a separately validated DVS module | Planned |
| M9 | Public release qualification | Clean-machine installer/portable verification, performance budget, soak tests, recovery tests, accessibility/i18n review, source/notices and signed release process decision | Planned |

## Next implementation priorities

1. Finish M1 manual validation: clean Windows 11 launch, resize/import behavior, actual audio-device switching and four-output cue on supported hardware. CI compilation alone does not close M1.
2. Replace the decoded-whole-track memory model with bounded streaming/read-ahead and cancellation-safe cache behavior for long files.
3. Build deterministic render fixtures and objective resampler/transition metrics, then continue toward time-stretch/key-lock rather than treating rate interpolation as final DJ-grade tempo processing.

## Audio quality hardening in progress

The original linear rate converter has been replaced in development with allocation-free four-point Catmull-Rom interpolation. Play/pause/seek discontinuities now use short transitions, and deck EQ/echo/drive parameters are smoothed instead of applying full-scale jumps sample-to-sample. These changes reduce obvious development-engine artifacts but do **not** prove transparent DSP, key lock, zero clicks on every file/device, or professional live readiness.

## Effects scope

Target roughly 30–40 different DSP effects across delay/reverb, modulation, beat manipulation and distortion/pitch categories. A preset is not a new effect. Each effect needs bypass/null or reference tests where meaningful, bounded parameters, stable tail handling and listening tests.

## Completion reporting

`docs/progress.json` is the single milestone counter. `python scripts/update_progress.py` regenerates `assets/readme/progress-card.svg` and `assets/readme/progress-mini.svg`; `--check` rejects stale output and legacy character meters in maintained README/roadmap dashboards. Ten equal milestones make the count easy to audit but **not a percentage of engineering effort or live-readiness**. Never advance it because a scheduled task ran.
