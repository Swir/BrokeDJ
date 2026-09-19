# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`.
- PR #27 (`feat/manual-grid-persistence-validation`) merged as `3ec1784541d5eeab987b6a3b544013722841865d` after exact-final-head `c97db7823cfd66306d9144c2f78f65ccebed51b3` passed GitHub Actions run `35418039710`.
- Run `35418039710` passed Linux ASan/UBSan plus the configured core/analysis/time-stretch tests, and Windows x64 configure/build/full CTest/audio diagnostics/native no-audio GUI smoke/staging/artifact upload.
- Roadmap counter remains **M0 complete; 1/10 equal-weight milestones = 10.0%**. The new persistence boundary is part of M2 groundwork and does not close M2.

## Verified manual beat-grid persistence boundary

PR #27 adds a separate persistent store for user-authored beat-grid corrections without turning detector output into mutable user state.

The merged boundary:

- stores only validated `BeatGrid` state: beat-zero plus bounded variable-tempo segments;
- keeps manual corrections separate from automatic BPM/key analysis cache records, so later detector re-analysis cannot silently masquerade as a user edit;
- ties each persisted correction to the current source file size and modification time; changed source identity invalidates the old correction instead of applying it to different audio;
- uses an opaque path-derived local filename while omitting the raw local path and filename from the persisted payload;
- treats local persisted state as untrusted on load and re-validates beat zero, BPM ranges, segment count/order and complete `BeatGrid` continuity before publishing it;
- writes through `juce::TemporaryFile` so a failed replacement does not intentionally destroy the previous valid record;
- supports explicit erase/reset of the stored override;
- remains outside the audio callback and adds no disk I/O, allocation or synchronization to realtime rendering.

Deterministic tests now cover a three-segment variable-tempo correction, exact enough beat/time round-trip, payload path privacy, malformed-write preservation, source-identity invalidation and erase behavior.

This PR establishes persistence and validation only. It does **not** expose a manual grid editor in the normal GUI yet, does not claim automatic detector accuracy on arbitrary music, and does not enable sync from unreviewed metadata.

## Validation state

- PR #27 exact-final-head run `35418039710` is green across Linux and Windows development gates.
- The new persistence tests execute through the real JUCE analysis-adapter target rather than a detached mock store.
- The audio callback topology is unchanged; manual-grid file I/O remains entirely off callback.
- The previous BPM/grid/key analysis and optional key-lock research paths remain available with their existing gates and limitations.
- No physical Windows 11 audio interface, controller, reviewed music-domain listening, measured device latency, hardware-underrun qualification or representative copyrighted-music corpus is claimed.
- `docs/progress.json` remains unchanged at 1/10 = 10.0%.

## Remaining blockers / gates

1. Physical Windows 11 clean-machine launch, real audio-device switching and physical two-/four-output cue still block M1 completion.
2. M2 still needs the actual manual beat-grid editing UX that writes the verified persistence boundary, plus clear reset/re-analysis behavior.
3. A reproducible representative-music validation harness and legally usable/local reference corpus are still needed before stronger BPM/key accuracy claims.
4. Grid-aware sync, hotcues and beat-length loops remain open; current LOOP still repeats the whole track.
5. The developer key-lock lifecycle still requires reviewed music-domain listening and stronger live transition/race qualification before any normal GUI control is justified.
6. Slip, reverse, scratch workflows and broader waveform/deck interaction remain open.

## Next highest-impact step

Connect the verified override store to a compact manual beat-grid editor and build the representative-music validation harness. Once corrected grids have a real user workflow and repeatable validation evidence, use those grids as the authority for sync, hotcue quantization and beat-length loops. Physical M1 audio-interface checks remain a separate manual gate and must not be inferred from CI.
