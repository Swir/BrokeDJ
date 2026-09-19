# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`; current integration branch: `feat/performance-deck-owner`.
- PR #28 (`feat/manual-grid-editor-validation`) merged as `f917fabc579723044be706e3eb86971cb48a67ad` after exact-final-head `62dc9e40b2b84769109613734bd21fb0916c11eb` passed GitHub Actions run `35421304445`.
- PR #29 (`feat/grid-performance-primitives`) merged as `473573962047d31b188915c3e47df79654baecf6` after exact-final-head `090513a718ff9a8a74c0bd418d9fc25d248c7a87` passed GitHub Actions run `35421909580`.
- PR #30 (`feat/engine-beat-loop-region`) merged as `e7d67e96aa514c9f229daba92e3fca849acb0522` after exact-final-head `b6bd9508e603b3c6ee6e5814cad73282429cf0f8` passed GitHub Actions run `35423792947` on Linux ASan/UBSan and the Windows x64 development gate.
- PR #31 (`feat/performance-deck-owner`) is open. Its code head `681240671b451cf19f71bd939f472a29ba6d7ded` passed GitHub Actions run `35426185917`: Linux ASan/UBSan completed 24/24 configured CTest targets, and Windows x64 completed configure/build/full CTest/audio diagnostics/native no-audio GUI smoke/staging/artifact upload.
- PR #31 is intentionally not merged merely because its code head is green; the branch remains the active M2 integration slice for the next coherent UI/owner work.
- Roadmap counter remains **M0 complete; 1/10 equal-weight milestones = 10.0%**. Manual grid tooling, performance planning, production loop-region rendering and the new owner boundary are substantial M2 work, but they do not close M2 or imply live readiness.

## Verified manual beat-grid workflow

PR #28 connects the persistence boundary to a compact per-deck manual workflow without moving analysis or persistence into the audio callback.

The merged workflow:

- exposes beat-zero and base-BPM correction controls only when a valid reviewed grid exists;
- persists corrections through the source-identity-bound local override store and supports explicit reset to detector output;
- keeps automatic detector results separate from user-authored overrides;
- renders beat/bar overlays from the selected grid over the waveform while keeping waveform amplitude data and beat metadata distinct;
- rejects invalid edits and re-publishes the previous valid grid instead of poisoning deck state;
- ships a local manifest-driven BPM/key analysis validator target for user-provided reference audio without redistributing copyrighted music;
- keeps analysis/cache/store work off the realtime callback.

The repository still does **not** claim representative-music BPM/key accuracy until a legally usable local corpus is actually run and reviewed.

## Verified reviewed-grid performance primitives

PR #29 adds JUCE-independent planning helpers that consume a validated `BeatGrid` without directly mutating live transport state.

The merged primitives:

- quantize candidate hotcue/seek positions to the previous, nearest or next musical beat;
- plan beat-length loop endpoints in beat space, so a loop crossing a variable-tempo segment keeps the requested musical length instead of assuming one fixed BPM duration;
- derive a bounded one-shot follower rate and phase target from two reviewed grids for later sync integration;
- expose signed phase error rather than hiding an automatic transport jump;
- fail closed for invalid grids, invalid quantization/loop lengths and tempo ratios outside the existing deck rate envelope;
- add no disk/network I/O, locking, allocation or transport mutation to the audio callback.

Deterministic tests cover reviewed-boundary BPM selection, directional quantization, a four-beat loop crossing a 120→90 BPM segment, compatible 120→128 BPM sync planning, signed phase correction and rejection of unsafe tempo ratios.

## Verified production beat-loop transport boundary

PR #30 wires reviewed `BeatLoopPlan` boundaries into a production Engine source path while preserving the callback contract.

The merged Engine boundary:

- owns one dormant loop-region renderer per deck instead of swapping a new renderer pointer while audio is running;
- accepts a bounded source-time start/end snapshot through lock-free atomics and fails closed for invalid ranges or decks;
- keeps the existing Catmull-Rom / prepared windowed-sinc conversion strategy inside the reviewed region;
- wraps interpolation taps inside the loop window and applies a short prepared transition at the wrap instead of a hard discontinuity;
- retains existing EQ/FX/gain/cue/crossfader/master/meter processing after source rendering;
- preserves streamed-cache starvation/refill diagnostics and falls back rather than performing I/O or blocking in the callback;
- binds an active region to the immutable clip observed when rendering begins so clip replacement cannot silently reuse a stale loop plan;
- refuses to arm the built-in loop-region path if an external source renderer such as the opt-in key-lock research path owns that deck.

Core tests build a real 120 BPM reviewed grid, derive a four-beat plan, arm the Engine, render through the wrap and verify bounded transport plus finite output. The zero-heap realtime contract now runs with an active custom beat loop while continuing to require zero callback heap allocations and deallocations.

This is a transport-engine capability. The existing GUI LOOP button still represents whole-track looping until the reviewed-grid owner is connected to explicit beat-length controls.

## Verified performance-deck owner candidate

PR #31 adds a JUCE-independent control-thread owner on top of the reviewed grid and production Engine transport boundary.

The green code-head candidate:

- keeps whole-track LOOP and reviewed beat-length loops as separate explicit modes instead of silently changing the meaning of the existing LOOP control;
- derives beat-loop regions from `BeatLoopPlan`, checks requested cursor/duration/length, rejects loops extending beyond the track and publishes the region before changing the Engine LOOP state;
- preserves the previous valid loop if a replacement request fails validation;
- invalidates an armed beat-derived region when its reviewed grid changes and clears source-identity-bound loop/grid/cue state on clip replacement;
- exposes eight in-memory hotcue slots with optional reviewed-grid quantization and normalized Engine seek triggering;
- conservatively exits an active beat loop before a hotcue jump while preserving explicit whole-track loop mode;
- refuses a beat-loop request if another source renderer, including the opt-in key-lock research path, currently owns that deck;
- performs no decoding, disk/network I/O, plugin work or callback allocation; it translates message/control-thread intent into the existing bounded Engine control interfaces.

`PerformanceDeckOwnerTests` deterministically cover variable-tempo beat-loop ownership, failed tail re-arm, grid replacement, whole-track/beat-loop separation, quantized and unquantized hotcues, renderer conflicts, invalid decks and clip-reset invalidation. This owner is not yet connected to normal GUI controls and hotcues are not yet persisted across sessions.

## Validation state

- PR #28 exact-final-head run `35421304445` is green; merged-main run `35421668659` is also green.
- PR #29 exact-final-head run `35421909580` is green on Linux and Windows. Linux reports 23/23 configured CTest targets passing.
- PR #30 exact-final-head run `35423792947` is green on Linux and Windows, including Windows audio diagnostics, native GUI smoke, staging and artifact upload.
- PR #31 code head `681240671b451cf19f71bd939f472a29ba6d7ded` passed run `35426185917`: Linux ASan/UBSan reports 24/24 configured CTest targets passing; Windows x64 passed its full development gate including audio diagnostics, native GUI smoke, staging and artifact upload.
- The optional key-lock research path retains its previous gates and is not automatically enabled by reviewed-grid, beat-loop or hotcue-owner work.
- No physical Windows 11 audio interface, controller, reviewed music-domain listening, measured device latency, hardware-underrun qualification or representative copyrighted-music corpus is claimed.
- `docs/progress.json` remains unchanged at 1/10 = 10.0%.

## Remaining blockers / gates

1. Physical Windows 11 clean-machine launch, real audio-device switching and physical two-/four-output cue still block M1 completion.
2. The analysis validator still needs an actual legally usable/local representative corpus and recorded results before stronger BPM/key accuracy claims.
3. The normal app UI does not yet expose reviewed beat-length loop choices through `PerformanceDeckOwner`; whole-track LOOP must retain its existing meaning until that explicit integration is complete and tested.
4. Hotcues are currently an in-memory owner contract only; persistence, normal UI pads, controller mapping and transport regression still need implementation and qualification.
5. Sync planning exists but production sync actuation still needs an explicit owner/UI contract, bounded rate/phase application, deterministic render tests and fail-safe behavior.
6. The developer key-lock lifecycle still requires reviewed music-domain listening and stronger live transition/race qualification before any normal GUI control is justified.
7. Slip, reverse, scratch workflows and broader waveform/deck interaction remain open.

## Next highest-impact step

Connect `PerformanceDeckOwner` to the normal application lifecycle and add compact explicit per-deck beat-loop controls for 1/2/4/8/16 beats without changing whole-track LOOP semantics. Re-publish the selected reviewed grid after analysis/manual edits and reset owner state on clip replacement. Then add normal hotcue pads/persistence and use the already-reviewed sync plan for bounded production sync actuation. Physical M1 audio-interface checks remain a separate manual gate and must not be inferred from CI.
