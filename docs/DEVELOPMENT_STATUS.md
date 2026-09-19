# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`.
- PR #25 (`feat/offline-rhythm-grid-integration`) merged as `d3f7e2e77bc61decd9defcabb94c166f0a6432bf` after exact-final-head `73e3b6464a734a89c7541ad67d3594ad5843bd72` passed its Linux + Windows gate. It established the background BPM/beat-grid worker integration, editable JUCE-independent variable-tempo `BeatGrid` foundation and per-deck analysis publication.
- PR #26 (`feat/offline-musical-key-analysis`) merged as `5200e13e25cde17776cebe7966fbcf16e6c47e69` after exact-final-head `d562cdeba5923f9c6019ba41d6b5ea36125bdbf1` passed GitHub Actions run `35415435491`.
- Run `35415435491` passed Linux ASan/UBSan plus the configured core tests, and Windows x64 configure/build/full CTest/audio diagnostics/native no-audio GUI smoke/staging/artifact upload.
- Roadmap counter remains **M0 complete; 1/10 equal-weight milestones = 10.0%**. M2 is materially more capable, but its full acceptance criteria remain open.

## Verified offline musical-analysis slice

PRs #24–#26 now provide a bounded worker-only musical-analysis path without moving file I/O, decoding, cache access or analysis into the audio callback.

The integrated slice now:

- performs cancellable offline BPM/beat-zero analysis after a successful track load without delaying playback availability;
- validates and publishes a JUCE-independent `BeatGrid` that supports beat↔time mapping, beat-zero/BPM correction and bounded variable-tempo segments for later manual editing, quantization and sync;
- adds a JUCE-independent musical-key accumulator that decimates input to a bounded analysis rate, builds fixed-window chroma evidence and compares it against major/minor key profiles;
- rejects silence, steady single-tone material and deterministic broadband-noise fixtures rather than inventing a key;
- sanitizes non-finite input and bounds analysis duration before publishing a result;
- feeds BPM/grid and key accumulators from one sequential background decode pass instead of doubling source-file I/O;
- stores independently optional validated beat/grid/key metadata in analysis-cache schema v2 while omitting the raw local pathname from the cache payload;
- invalidates stale cache records when source size/modification identity changes and rejects malformed grid/key metadata before publication;
- exposes independently valid BPM/grid/key metadata in each deck status, so a rejected tempo does not discard a valid key and vice versa;
- keeps key confidence and detector limitations visible in the tooltip instead of implying arbitrary-music accuracy.

The current key detector uses deterministic synthetic evidence only. It is not yet evidence of professional key-detection accuracy, harmonic-mixing correctness or reliable behavior across a representative music corpus.

## Validation state

- PR #26 exact-final-head run `35415435491` is green across Linux and Windows development gates.
- Linux sanitizer validation covers the new core key-analysis target in addition to the existing engine/rhythm/grid/realtime contracts.
- Windows x64 completed configure/build, full CTest including track-analysis/cache integration, audio diagnostic replay, native no-audio GUI lifecycle smoke, staging and artifact upload on the same exact head.
- The audio callback topology is unchanged by offline musical analysis. Analysis and cache work remain off callback.
- No physical Windows 11 audio interface, controller, reviewed music-domain listening, measured device latency, hardware-underrun qualification or arbitrary-music analysis benchmark is claimed.
- `docs/progress.json` remains unchanged at 1/10 = 10.0%; neither the analysis slice nor key-lock research closes the broader M2 performance-deck milestone.

## Remaining blockers / gates

1. Physical Windows 11 clean-machine launch, real audio-device switching and physical two-/four-output cue still block M1 completion.
2. M2 still needs representative real-music BPM/key validation, manual beat-grid correction/persistence and clear recovery when metadata is wrong.
3. Grid-aware sync, hotcues and beat-length loops remain open; current LOOP still repeats the whole track.
4. The developer key-lock lifecycle still requires reviewed music-domain listening and stronger live transition/race qualification before any normal GUI control is justified.
5. Slip, reverse, scratch workflows and broader waveform/deck interaction remain open.
6. The current confidence values are detector-specific regression signals, not calibrated probabilities of musical correctness.

## Next highest-impact step

Build the manual beat-grid correction/persistence boundary and a reproducible representative-music validation harness for BPM/key metadata, then use corrected grids as the authority for sync, hotcue quantization and beat-length loops. Physical M1 audio-interface checks remain a separate manual gate and must not be inferred from CI.
