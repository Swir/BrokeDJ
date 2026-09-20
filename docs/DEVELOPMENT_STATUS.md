# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch: `main` at `5fda9cb6029dc610769d976c5b5939c27d85c03c` (`M2: harden key-lock transport and device transitions`).
- PR #45 was merged only after exact-head `5e9cf0b1c279439bca97eaf2706d0a85ba6a4b9f` passed Build and test run `35504664596`: Linux sanitizer/progress/full configured CTest and Windows x64 configure/build/full CTest/audio diagnostics/native GUI smoke/silent device probe/staging/artifact upload all succeeded.
- Active development branch: `feat/m2-multideck-keylock-stress`.
- Active pull request: #46 (`M2: add multi-deck key-lock transition isolation coverage`).
- First implementation checkpoint: `2ece849c2bc0ce4f761d7fac2ff271d521ac4e40`; exact-head Build and test run `35507464963` is queued and must pass before this package can merge.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA).
- GitHub Releases is empty; no public BrokeDJ Release exists or is qualified by this checkpoint.

## Newly integrated on main: key-lock transition hardening

1. **Authoritative transport snapshot validation**
   - `KeyLockDeckLifecycle::service()` compares current loop/rate controls with the last successfully staged optional key-lock snapshot.
   - Unannounced controller/device changes fail closed to the ordinary production renderer and become eligible for a paused restage instead of leaving a stale research snapshot armed.

2. **Sticky invalid-pitch validation**
   - Out-of-range/non-finite pitch intent latches the optional path invalid until a later explicit valid request.
   - Re-applying the previous valid pitch is a deterministic recovery action; the timer cannot silently resurrect a rejected value.

3. **Idempotent disarm and production-owner restoration**
   - Repeated disabled/timer polling no longer manufactures publication-generation churn.
   - Removing the research renderer restores Engine's built-in transport owner, preserving production Beat Loop and Reverse/Slip behavior.

## Current PR #46: four-deck transition isolation

1. **All four optional owners are exercised together**
   - The lifecycle fixture now submits and stages four independent immutable clips, starts all four decks and requires every optional renderer to accept the initial callback window.

2. **Deck-local failures stay deck-local**
   - An unannounced rate change on deck B must disarm only B while A/C/D remain armed, retain their generation and continue accepting render blocks.
   - An invalid pitch on deck C must remain sticky/fail-closed only on C until an explicit valid recovery, with A/B/D continuing normally.
   - Disabling/re-enabling deck D must not churn the other owners.

3. **Device transitions preserve intent without stale DSP state**
   - The test stops all decks, releases the research owners, verifies all four become dirty/unarmed, rejects an invalid 1 kHz device configuration, then re-prepares Engine/lifecycle at 44.1 kHz and requires all four deck intents to restage independently.
   - Invalid deck indexes are also rejected fail-closed.

## Validation evidence and gates still open

- PR #46 exact-head run `35507464963` is queued for Linux ASan/UBSan + generated-progress + full configured CTest and Windows x64 configure/build/full CTest/audio diagnostics/native GUI smoke/silent device probe/staging.
- Real Windows 11 clean-machine/manual resize/HiDPI/device-switching validation remains open.
- Independent master 1/2 and cue 3/4 still require a real four-output interface and listening verification.
- Physical slow-storage, multi-minute real-world compressed files and hardware underrun behavior remain open; controlled decoder fixtures are not physical-storage qualification.
- Native MIDI/controller mappings and concrete controller profiles remain unqualified; the current native jog input is mouse/native-GUI only.
- Representative music-domain BPM/key/grid evidence, production key-lock listening/latency, wider scratch/controller evidence, multi-hour soak and public-release qualification remain open.

## Documentation drift found during this audit

`README.md`, `ROADMAP.md` and `docs/ARCHITECTURE.md` still contain older wording saying later variable-tempo segment controls are not exposed and/or that Slip/Reverse/Scratch are wholly open. The current source already contains `TempoSegmentEditorComponent`, native reviewed-grid add/move/replace/remove plus undo/redo wiring, four-deck REV/SLIP controls and the bounded native JOG/SCRATCH strip. Those documents need a truthful synchronization pass without changing `docs/progress.json` or claiming production key-lock/controller/hardware qualification.

## Next largest step

Fix any exact-head regression in PR #46 first. If the new four-deck fixture is green, synchronize the stale M2 documentation claims, then continue the highest-value unblocked M2 work: representative analysis evidence and key-lock/controller transition qualification while preserving the manual M1 hardware gates.