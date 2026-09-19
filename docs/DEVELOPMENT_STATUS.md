# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`.
- PR #28 (`feat/manual-grid-editor-validation`) merged as `f917fabc579723044be706e3eb86971cb48a67ad` after exact-final-head `62dc9e40b2b84769109613734bd21fb0916c11eb` passed GitHub Actions run `35421304445`.
- PR #29 (`feat/grid-performance-primitives`) merged as `473573962047d31b188915c3e47df79654baecf6` after exact-final-head `090513a718ff9a8a74c0bd418d9fc25d248c7a87` passed GitHub Actions run `35421909580`.
- PR #30 (`feat/engine-beat-loop-region`) merged as `e7d67e96aa514c9f229daba92e3fca849acb0522` after exact-final-head `b6bd9508e603b3c6ee6e5814cad73282429cf0f8` passed GitHub Actions run `35423792947` on Linux ASan/UBSan and the Windows x64 development gate.
- The PR #30 Windows gate included configure/build/full CTest, audio diagnostics, native no-audio GUI smoke, staging and artifact upload. Its Linux job passed the generated-progress check, sanitizer build and full configured CTest suite.
- Roadmap counter remains **M0 complete; 1/10 equal-weight milestones = 10.0%**. Manual grid tooling, performance planning and production loop-region rendering are substantial M2 work, but they do not close M2 or imply live readiness.

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

This is a transport-engine capability, not yet a normal user-facing beat-loop control. The existing GUI LOOP button still represents whole-track looping until an explicit reviewed-grid UI/owner contract arms a custom region.

## Validation state

- PR #28 exact-final-head run `35421304445` is green; merged-main run `35421668659` is also green.
- PR #29 exact-final-head run `35421909580` is green on Linux and Windows. Linux reports 23/23 configured CTest targets passing.
- PR #30 exact-final-head run `35423792947` is green on Linux and Windows, including Windows audio diagnostics, native GUI smoke, staging and artifact upload.
- Beat-loop source rendering is now present in production `Engine`; user-facing beat-loop selection, hotcue storage/triggering and sync actuation remain open.
- The optional key-lock research path retains its previous gates and is not automatically enabled by reviewed-grid or beat-loop work.
- No physical Windows 11 audio interface, controller, reviewed music-domain listening, measured device latency, hardware-underrun qualification or representative copyrighted-music corpus is claimed.
- `docs/progress.json` remains unchanged at 1/10 = 10.0%.

## Remaining blockers / gates

1. Physical Windows 11 clean-machine launch, real audio-device switching and physical two-/four-output cue still block M1 completion.
2. The analysis validator still needs an actual legally usable/local representative corpus and recorded results before stronger BPM/key accuracy claims.
3. The UI/owner layer does not yet expose reviewed beat-length loop choices and must not silently reinterpret the existing whole-track LOOP control.
4. Hotcue persistence/triggering and sync actuation still need explicit owner/UI/transport contracts, deterministic render tests and fail-safe behavior before user-facing enablement.
5. The developer key-lock lifecycle still requires reviewed music-domain listening and stronger live transition/race qualification before any normal GUI control is justified.
6. Slip, reverse, scratch workflows and broader waveform/deck interaction remain open.

## Next highest-impact step

Add a compact reviewed-grid beat-loop owner/UI contract that derives bounded lengths (for example 1/2/4/8/16 beats) through `BeatLoopPlan` and arms/disarms the verified Engine region without changing the semantics of whole-track LOOP unexpectedly. Then reuse the same reviewed-grid authority for quantized hotcue storage/triggering and explicit sync actuation. Physical M1 audio-interface checks remain a separate manual gate and must not be inferred from CI.
