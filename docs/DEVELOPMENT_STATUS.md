# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`
- Active development branch: `feature/stream-underrun-diagnostics`
- Pull request: `#5` — open; exact-final-head CI required before merge
- Current development head before this status commit: `88fac0331d4e443ecf08e49e2816406938cec605`
- Roadmap counter remains: **M0 complete; 1/10 equal-weight milestones = 10.0%**

## Completed in the seek/refill hardening package

- Added a bounded, non-audio `StreamCacheDiagnostics` snapshot that exposes the requested frame/chunk, exact requested-region residency and forward cache coverage without claiming a physical audio underrun.
- Changed the background read-ahead order so a seek refills the exact requested chunk before forward prefetch.
- Prevented out-of-range chunks beyond EOF from being counted as successful read-ahead work, eliminating a potential end-of-track worker busy loop.
- Expanded `brokedj_stream_cache_tests` with a virtual 90-minute stream, repeated distant seeks, bounded finite-output checks, streamed whole-track loop wrap and deterministic starvation/refill snapshots without allocating full-track audio.
- Preserved the real-time boundary: no new file I/O, decode work, allocation, logging or blocking synchronization was added to the audio callback.

## Validation state

- An earlier PR #5 head passed the Linux ASan/UBSan core job before documentation and read-ahead scheduling hardening were added.
- The final branch head still requires the complete Linux sanitizer + Windows x64 build/CTest/native no-audio GUI-smoke workflow before merge.
- No physical audio interface, Windows 11 clean-machine, slow-storage or reviewed listening validation was performed by this package.

## Remaining blockers / gates

1. Clean Windows 11 interactive launch, resize/import and actual audio interface behavior still require manual verification.
2. Long-file behavior still needs real WAV/FLAC/OGG/MP3 seek/loop, slow-storage and reviewed listening evidence.
3. Cache-starvation event/history counters and refill-onset smoothing remain open; the new snapshot is current-state observability, not an underrun certificate.
4. Objective render fixtures and reviewed listening tests are needed before stronger sound-quality claims.
5. Time-stretch/key-lock, beat analysis/grid and the rest of M2 remain open.

## Next highest-impact step

After exact-final-head CI passes and PR #5 is merged, add cache-starvation event/history accounting plus refill-onset smoothing, then exercise real supported-codec seek/loop fixtures before moving deeper into beat/key processing.
