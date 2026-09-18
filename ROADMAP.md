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
| M2 | Performance decks | Validated beat/tempo/key analysis, editable grids including tempo changes, key lock, hotcues, beat loops, slip/reverse/scratch and de-clicked transitions | De-clicking, smoothed rate changes, improved interpolation and bounded long-track read-ahead are implemented in development; milestone still open |
| M3 | Professional mixer and recording | Configurable routing, gain staging, EQ curves, fader laws, limiter evaluation, microphone/ducking and dropout-aware set recording | Basic source only; full milestone open |
| M4 | Music library and sessions | SQLite migrations, search/tags/playlists/history, duplicate and moved-file handling, waveform/analysis cache, session persistence and tested backup restore | Planned |
| M5 | Effects and VST3 | Distinct effect inventory, chains/sends, presets, XY/macros, automation smoothing, scanner isolation, license review and a tested compatibility matrix | Two built-in effects; parameter smoothing added; milestone open |
| M6 | Sampler and controllers | User sample/loop banks, live capture, quantized triggers, MIDI Learn and verified controller mappings | Planned |
| M7 | Stems and creative assistance | Separate stem decks, tested offline separation, optional live processing with published hardware limits, user-controllable set suggestions | Planned |
| M8 | Advanced interoperability | Documented app synchronization, streaming routes and a separately validated DVS module | Planned |
| M9 | Public release qualification | Clean-machine installer/portable verification, performance budget, soak tests, recovery tests, accessibility/i18n review, source/notices and signed release process decision | Planned |

## Next implementation priorities

1. Finish M1 manual validation: clean Windows 11 launch, resize/import behavior, actual audio-device switching and four-output cue on supported hardware. CI compilation alone does not close M1.
2. Harden bounded read-ahead with explicit underrun/onset diagnostics, long-file seek/loop stress fixtures and slow-storage behavior so a cache miss never becomes a hidden quality claim.
3. Build deterministic render fixtures and objective resampler/transition metrics, then continue toward time-stretch/key-lock rather than treating rate interpolation as final DJ-grade tempo processing.

## Audio quality and long-track hardening in progress

The original linear rate converter has been replaced in development with allocation-free four-point Catmull-Rom interpolation. Play/pause/seek and whole-track loop wraparound use short de-click transitions. Playback-rate targets, headphone cue switching/level and deck EQ/echo/drive controls are smoothed rather than applying abrupt sample-to-sample jumps.

Large tracks no longer require one decoded stereo allocation for the full file. The development path uses a fixed-size stream cache populated by a background JUCE reader, with small files retaining the simpler in-memory path. Cache misses request the new source region and return bounded silence instead of doing I/O in the callback. This removes the previous fixed decoded-size ceiling for the streaming path, but seek recovery, cache-refill onset, slow storage, compressed-codec behavior and long loop edges still require stress/listening evidence.

These changes do **not** prove transparent DSP, key lock, zero clicks/dropouts on every file/device, or professional live readiness.

## Effects scope

Target roughly 30–40 different DSP effects across delay/reverb, modulation, beat manipulation and distortion/pitch categories. A preset is not a new effect. Each effect needs bypass/null or reference tests where meaningful, bounded parameters, stable tail handling and listening tests.

## Completion reporting

`docs/progress.json` is the single milestone counter. `python scripts/update_progress.py` regenerates `assets/readme/progress-card.svg` and `assets/readme/progress-mini.svg`; `--check` rejects stale output and legacy character meters in maintained README/roadmap dashboards. Ten equal milestones make the count easy to audit but **not a percentage of engineering effort or live-readiness**. Never advance it because a scheduled task ran.
