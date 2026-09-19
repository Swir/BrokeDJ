# BrokeDJ development status

This file is a durable engineering checkpoint, not a release announcement.

## Current checkpoint

- Default branch: `main`.
- PR #28 (`feat/manual-grid-editor-validation`) merged as `f917fabc579723044be706e3eb86971cb48a67ad` after exact-final-head `62dc9e40b2b84769109613734bd21fb0916c11eb` passed GitHub Actions run `35421304445`.
- Post-merge `main` run `35421668659` is green across Linux ASan/UBSan and Windows x64 configure/build/full CTest/audio diagnostics/native no-audio GUI smoke/staging/artifact upload.
- PR #29 (`feat/grid-performance-primitives`) merged as `473573962047d31b188915c3e47df79654baecf6` after exact-final-head `090513a718ff9a8a74c0bd418d9fc25d248c7a87` passed GitHub Actions run `35421909580`.
- Run `35421909580` passed all 23 configured Linux CTest targets under ASan/UBSan plus the Windows x64 development gate, including full CTest, audio diagnostics, native no-audio GUI smoke, staging and artifact upload.
- Roadmap counter remains **M0 complete; 1/10 equal-weight milestones = 10.0%**. Manual grid tooling and performance planning are substantial M2 work, but they do not close M2 or imply live readiness.

## Verified manual beat-grid workflow

PR #28 connects the previously qualified persistence boundary to a compact per-deck manual workflow without moving analysis or persistence into the audio callback.

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

## Validation state

- PR #28 exact-final-head run `35421304445` is green; merged-main run `35421668659` is also green.
- PR #29 exact-final-head run `35421909580` is green on Linux and Windows. Linux reports 23/23 CTest targets passing.
- The callback topology is unchanged by the new planning helpers; actual beat-loop wrapping, hotcue storage and sync actuation are not yet enabled in the production transport.
- The optional key-lock research path retains its previous gates and is not automatically enabled by reviewed-grid planning.
- No physical Windows 11 audio interface, controller, reviewed music-domain listening, measured device latency, hardware-underrun qualification or representative copyrighted-music corpus is claimed.
- `docs/progress.json` remains unchanged at 1/10 = 10.0%.

## Remaining blockers / gates

1. Physical Windows 11 clean-machine launch, real audio-device switching and physical two-/four-output cue still block M1 completion.
2. The analysis validator still needs an actual legally usable/local representative corpus and recorded results before stronger BPM/key accuracy claims.
3. Beat-length loop plans are not yet wired into `Engine` loop-region rendering; the normal LOOP control still repeats the whole track.
4. Hotcue persistence/triggering and sync actuation still need explicit owner/UI/transport contracts, deterministic render tests and fail-safe behavior before user-facing enablement.
5. The developer key-lock lifecycle still requires reviewed music-domain listening and stronger live transition/race qualification before any normal GUI control is justified.
6. Slip, reverse, scratch workflows and broader waveform/deck interaction remain open.

## Next highest-impact step

Wire a bounded custom loop region into the production `Engine` so a reviewed `BeatLoopPlan` can drive a real beat-length loop with deterministic boundary/de-click tests and immediate fallback. Then reuse the same reviewed-grid authority for quantized hotcue storage/triggering and explicit sync actuation. Physical M1 audio-interface checks remain a separate manual gate and must not be inferred from CI.
