# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness or live-performance claim.

## Current checkpoint

- Default branch: `main` at merged PR #36 checkpoint `95792fdbbb63f756e04b826db5c8ed70e1507ae3`.
- PR #36 (`M1: add silent audio-device witness probe`) final head `cbffd31b66303600d8dd0569a117c4826a2575ec` passed exact-head GitHub Actions run `35466737115`, then merged to `main`; merged-main run `35469501191` also passed.
- Active development branch: `feat/m2-tempo-segment-owner`.
- Current package adds transactional variable-tempo segment boundary operations to the JUCE-independent `BeatGrid`, routes reviewed-grid segment mutation through `PerformanceDeckOwner`, and extends deterministic core tests. A pull request / exact-head gate is required before this work can be integrated.
- Roadmap source of truth remains `docs/progress.json`: 1/10 milestones complete (10.0%, PRE-ALPHA).
- No public BrokeDJ Release exists or is qualified by this checkpoint.

## Newly integrated on main: PR #36

1. **Silent audio-device capability probe**
   - `BrokeDJ.exe --device-probe` writes both TXT and schema-versioned JSON capability evidence without calling `AudioIODevice::open`, starting a callback or emitting sound.
   - The full local probe checks descriptor open state before and after capability queries and fails closed if the non-opening invariant is violated.
   - Four-output descriptors remain candidates only; they are not physical cue-routing certification.

2. **Machine-readable CI contract**
   - `--device-probe-ci` performs enumeration-only discovery without constructing per-device descriptors.
   - Windows CI parses the JSON contract, verifies schema/safety booleans and rejects descriptor records in CI mode.
   - Merged-main run `35469501191` passed the Linux sanitizer/progress gate and the Windows x64 build/test/GUI-smoke/device-probe/staging gate for `95792fdbbb63f756e04b826db5c8ed70e1507ae3`.

3. **Manual M1 witness procedure**
   - `docs/M1_HARDWARE_WITNESS.md` defines clean launch, resize/HiDPI, import/playback, device switching and physical outputs 1/2 versus cue 3/4 evidence.
   - The procedure still requires a real Windows 11 machine and audio interface; automated enumeration is not treated as hardware qualification.

## Current development package: variable-tempo edit ownership

1. **Transactional tempo-boundary primitives**
   - `BeatGrid::moveTempoChangeToBeat` moves any non-base tempo boundary to an exact musical beat using a candidate copy and commits only after complete validation.
   - `BeatGrid::replaceTempoChange` combines boundary movement and BPM replacement transactionally.
   - `BeatGrid::tempoChangeBeat` exposes a validated segment boundary in musical beat space for future UI/controller use.
   - Segment zero remains the beat-zero boundary and cannot be moved or removed.

2. **Performance-owner mutation boundary**
   - `PerformanceDeckOwner` now owns reviewed segment BPM edits, insertion, movement, combined replacement and removal instead of allowing a UI/controller layer to mutate the live reviewed grid behind the performance boundary.
   - Successful grid replacement invalidates an armed beat-derived loop whose source-time bounds came from the previous map; rejected edits leave the prior reviewed grid and loop state untouched.
   - These operations remain message/control-thread work. They add no disk/network I/O, decoding, locks or unbounded work to `Engine::process()`.

3. **Deterministic regression coverage**
   - Core tests cover exact boundary-beat reporting, successful boundary movement/replacement, base-boundary protection, duplicate/invalid edit rejection and transactional state preservation.
   - Owner-level tests cover insert/edit/move/remove operations and fail-closed behavior across the reviewed-grid boundary.
   - Existing `TrackBeatGridOverrideStore` tests already prove that multi-segment manual grids round-trip atomically, remain source-identity-bound and omit raw local paths/names from persisted payloads.

## Gates still open

- Exact-head Linux/Windows CI for the active tempo-segment owner branch before merge.
- Native compact UI/controller controls for selecting, adding, moving and removing later tempo segments; this package deliberately does not claim that full segment editing is user-facing yet.
- Real Windows 11 clean-machine/audio-interface validation, including actual device switching and independent four-output cue where supported.
- Manual resize/HiDPI usability review of the dense four-deck layout.
- Representative legal local-music corpus validation for BPM/key/grid behavior, especially genuinely variable-tempo material.
- Real listening/soak, controller and production-release qualification.
- Production key lock, slip/reverse/scratch and broader M2 workflow completion.

## Next largest step

Require exact-head CI for the tempo-segment owner package first. If green, expose a compact native segment editor that uses this owner boundary and the already-existing `TrackBeatGridOverrideStore` persistence path, with safe add/move/remove actions and no new work in the audio callback. Then validate edited variable-tempo grids against a representative local corpus before making stronger detector or Sync claims.
