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
| M2 | Performance decks | Validated beat/tempo/key analysis, editable grids including tempo changes, key lock, hotcues, beat loops, slip/reverse/scratch and de-clicked transitions | De-clicking, smoothed rate changes, hybrid Catmull-Rom/band-limited windowed-sinc rate conversion, bounded long-track read-ahead, codec stress, starvation/refill telemetry and objective render/realtime tests are implemented. The opt-in key-lock research stack now includes processor, bounded deck clock, real Clip/StreamCache source bridge and a device-rate/FIFO bridge with explicit production fallback under PR #16 qualification; it is still not connected to production deck playback and the milestone remains open. |
| M3 | Professional mixer and recording | Configurable routing, gain staging, EQ curves, fader laws, limiter evaluation, microphone/ducking and dropout-aware set recording | Basic source only; full milestone open |
| M4 | Music library and sessions | SQLite migrations, search/tags/playlists/history, duplicate and moved-file handling, waveform/analysis cache, session persistence and tested backup restore | Planned |
| M5 | Effects and VST3 | Distinct effect inventory, chains/sends, presets, XY/macros, automation smoothing, scanner isolation, license review and a tested compatibility matrix | Two built-in effects; parameter smoothing added; milestone open |
| M6 | Sampler and controllers | User sample/loop banks, live capture, quantized triggers, MIDI Learn and verified controller mappings | Planned |
| M7 | Stems and creative assistance | Separate stem decks, tested offline separation, optional live processing with published hardware limits, user-controllable set suggestions | Planned |
| M8 | Advanced interoperability | Documented app synchronization, streaming routes and a separately validated DVS module | Planned |
| M9 | Public release qualification | Clean-machine installer/portable verification, performance budget, soak tests, recovery tests, accessibility/i18n review, source/notices and signed release process decision | Planned |

## Next implementation priorities

1. Finish M1 manual validation: clean Windows 11 launch, resize/import behavior, actual audio-device switching and four-output cue on supported hardware. CI compilation alone does not close M1.
2. Expand long-file evidence beyond generated fixtures: broader mono/stereo WAV/AIFF/FLAC/OGG and CBR/VBR MP3 varieties, damaged/truncated cases and slow physical storage while preserving bounded memory and cancellation behavior.
3. Qualify PR #16 on its exact final head. Then design the smallest opt-in Engine-facing integration around the device bridge: prepare/re-prime outside the callback, apply reported processor/SRC latency to deck scheduling, render the current production converter in parallel as fallback, and gate enable/bypass/seek/loop/load/starvation transitions with deterministic render fixtures before exposing any UI control.

## Audio quality and long-track hardening in progress

The original linear rate converter has been replaced in development by a hybrid path: four-point Catmull-Rom interpolation where the effective source step does not require downsampling, and a prepared 24-tap Blackman-windowed sinc kernel selected from a finite cutoff/phase lookup bank when speed-up/downsampling needs anti-alias filtering. Kernel generation is outside the realtime callback. Play/pause/seek, whole-track loop wraparound and streamed starvation/refill boundaries use short de-click transitions. Playback-rate targets, headphone cue switching/level and deck EQ/echo/drive controls are smoothed rather than applying abrupt sample-to-sample jumps.

Large tracks no longer require one decoded stereo allocation for the full file. The development path uses a fixed-size stream cache populated by a background JUCE reader, with small files retaining the simpler in-memory path. After a seek, the reader prioritizes the exact requested chunk and its immediate previous/next neighbours before deeper forward read-ahead, then skips invalid chunks beyond EOF so end-of-track windows cannot create a false-work busy loop. Cache misses request the new source region and return bounded silence instead of doing I/O in the callback. Sparse waveform preview generation reads bounded windows off the audio thread instead of allocating the full decoded track.

Lock-free diagnostics track failed cache reads, the latest missed frame and collapsed starvation/refill episodes. Continuous missing data counts as one starvation episode until a complete interpolation frame becomes readable again. The same prepared transition window fades toward silence on starvation and fades recovered audio back in. Deterministic tests exercise a virtual 90-minute stream, repeated distant seeks, whole-track loop wrap and intentional starvation/refill recovery without allocating full-track audio.

The decoder/read-ahead adapter is separately testable with generated WAV/AIFF/FLAC/OGG data plus an original synthetic MP3 fixture, forced-streaming coverage, Unicode paths, invalid files, cancellation and controlled slow-reader behavior. Objective offline render tests measure rate-conversion frequency error/residual/DC/level, adjacent-sample transition deltas and anti-alias passband/stopband behavior across 48 kHz 1.5x, 44.1→48 kHz 1.5x, 96→48 kHz and 192→48 kHz paths. A separate realtime-contract target verifies zero heap allocation/deallocation in measured callback windows and records representative Catmull-Rom/sinc callback cost as diagnostic evidence. Windows development artifacts retain these measurements in `AUDIO-DIAGNOSTICS.txt`; shared-runner timing is not a performance certification.

The opt-in time-stretch research path remains outside ordinary playback. Its four JUCE-independent layers cover processor configuration/latency metadata, bounded fractional deck clocking, exact bounded reads from real in-memory or streamed BrokeDJ sources, and explicit conversion from stretched source-rate output to device-rate output through a bounded FIFO and prepared band-limited SRC. The device bridge consumes caller-provided production fallback samples, keeps audible transport separate from prefetch, exposes algorithm-latency metadata and fails back rather than emitting stale FIFO data after bypass/discontinuity/control changes. Deterministic tests cover independent time ratio/pitch, reset/seek history, fractional transport, loop/EOF behavior, cache starvation/refill, 44.1→48 and 96→48 conversion, fallback behavior and warmed zero-heap processing. Passing these tests establishes only a candidate integration path; it does not establish Engine playback, fully compensated deck latency, seamless perceptual switching, listening quality, supported hardware latency or release readiness.

This still does **not** prove transparent DSP, zero dropouts on real storage/codecs, production key lock, zero clicks on every transition, or professional live readiness. Real-file stress, slow-storage behavior, broader music-domain analysis, physical callback deadlines and reviewed listening evidence remain open.

## Effects scope

Target roughly 30–40 different DSP effects across delay/reverb, modulation, beat manipulation and distortion/pitch categories. A preset is not a new effect. Each effect needs bypass/null or reference tests where meaningful, bounded parameters, stable tail handling and listening tests.

## Completion reporting

`docs/progress.json` is the single milestone counter. `python scripts/update_progress.py` regenerates `assets/readme/progress-card.svg` and `assets/readme/progress-mini.svg`; `--check` rejects stale output and legacy character meters in maintained README/roadmap dashboards. Ten equal milestones make the count easy to audit but **not a percentage of engineering effort or live-readiness**. Never advance it because a scheduled task ran.
