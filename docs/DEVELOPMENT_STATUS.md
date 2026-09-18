# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`
- Active development branch: `feat/decoder-codec-stress`
- Pull request: `#7` — codec streaming/read-ahead hardening; merge remains gated on exact-final-head CI
- Latest implementation head validated before documentation-only checkpoint updates: `0da69e30c7c0a2fee72ae414e640d292c4b9e3fc`
- Validation run: `35346407766` — Linux ASan/UBSan core/SVG checks and Windows x64 configure/build/CTest/native no-audio GUI-smoke/staging passed
- Roadmap counter remains: **M0 complete; 1/10 equal-weight milestones = 10.0%**

## Completed in the codec streaming stress package

- Split the JUCE decoder/read-ahead adapter into its own testable target without changing production decode defaults.
- Added original deterministic WAV, AIFF, FLAC and OGG fixtures plus an embedded original synthetic MP3 fixture; tests exercise both in-memory and forced-streaming decode paths.
- Added Unicode-path coverage, distant streamed seeks, invalid-file handling and import cancellation checks.
- Added a controlled slow-reader mode used only by tests, plus last-request-wins read-ahead preemption so a new seek does not wait for an obsolete forward window.
- Sanitized non-finite streamed samples before publishing cache chunks.
- Fixed a Windows MP3 streaming regression exposed by the new tests: waveform generation now falls back from unsupported sparse seek patterns to a bounded-memory sequential pass on a fresh decoder, and playback/cache priming always receives another fresh decoder instance.
- Preserved the real-time boundary: the audio callback still performs no file I/O, decode work, allocation, logging or blocking synchronization.

## Validation state

- Initial PR #7 Windows CI exposed a real MP3 forced-streaming failure while Linux sanitizer/core checks remained green; the PR was not merged.
- After hardening preview/decoder separation, run `35346407766` passed both required jobs: Linux sanitizer/core/progress checks and Windows x64 build, all CTest targets including decoder fixtures, no-audio GUI smoke, staging and artifact upload.
- Documentation-only checkpoint updates still require a fresh exact-final-head PR run before merge.
- No physical audio interface, Windows 11 clean-machine, controller, slow physical storage or reviewed listening validation was performed by this package.

## Remaining blockers / gates

1. Clean Windows 11 interactive launch, resize/import and actual audio interface behavior still require manual verification.
2. The automated codec matrix is deterministic and exercises real JUCE codec readers, but multi-minute real-world files, CBR/VBR MP3 varieties, damaged/truncated variants and slow physical storage still need broader stress evidence.
3. The starvation/refill counters are cache/playback diagnostics, not physical device underrun counters.
4. Objective render fixtures and reviewed listening tests are needed before stronger sound-quality claims.
5. Time-stretch/key-lock, beat analysis/grid and the rest of M2 remain open.

## Next highest-impact step

Finish PR #7 on an exact-final-head green run, then add objective offline render metrics for resampling/transport transitions and expand the codec corpus without weakening the separate Windows 11/audio-hardware gate.
