# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`
- Active branch: `feat/stream-starvation-smoothing`
- Active scope: lock-free cache starvation history, complete-tap stream reads and refill transition smoothing
- Validation state: feature branch pending exact-final-head GitHub Actions before merge
- Previous merged seek/refill hardening: PR `#5`, merge `7cced0d18fe7d192fa983b482d2637b2204fe0f5`
- Roadmap counter remains: **M0 complete; 1/10 equal-weight milestones = 10.0%**

## Completed on the active branch

- Added lock-free totals for failed cache reads, starvation episodes and successful refill episodes plus the latest missed source frame.
- Kept cache-probe misses separate from playback starvation: the engine classifies one continuous unavailable period as one episode rather than counting every Catmull-Rom tap.
- Made streamed Catmull-Rom reads readiness-aware. If any required stereo interpolation tap is unavailable, the frame is treated as unavailable rather than combining cached samples with implicit zeros.
- Added a short transition toward silence when starvation begins and a short refill-onset fade when a complete interpolation frame becomes readable again.
- Expanded deterministic stream tests to check event accounting, recovery accounting and refill fade behavior in addition to the existing virtual 90-minute seek/loop stress.
- Preserved the real-time boundary: no file I/O, decode work, allocation, logging or blocking synchronization was added to the audio callback.

## Validation state

- Source implementation and deterministic tests are committed on the feature branch.
- Exact-final-head Linux ASan/UBSan and Windows x64 build/CTest/GUI-smoke/staging validation is required before merge.
- No physical audio interface, Windows 11 clean-machine, slow-storage or reviewed listening validation has been performed by this package.

## Remaining blockers / gates

1. Clean Windows 11 interactive launch, resize/import and actual audio interface behavior still require manual verification.
2. Long-file behavior still needs real WAV/AIFF/FLAC/OGG/MP3 seek/loop, intentionally slow-reader/storage stress and reviewed listening evidence.
3. The new starvation/refill counters are cache/playback diagnostics, not physical device underrun counters.
4. Objective render fixtures and reviewed listening tests are needed before stronger sound-quality claims.
5. Time-stretch/key-lock, beat analysis/grid and the rest of M2 remain open.

## Next highest-impact step

After exact-head CI passes and this branch merges, add real supported-codec long-file fixtures plus controlled slow-reader stress, then correlate the new starvation/refill history with deterministic delayed-input tests before moving deeper into beat/key processing.
