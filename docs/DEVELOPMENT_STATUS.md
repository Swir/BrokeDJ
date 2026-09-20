# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch: `main` at `5fda9cb6029dc610769d976c5b5939c27d85c03c` (`M2: harden key-lock transport and device transitions`).
- Active development branch: `feat/m2-multideck-keylock-stress`.
- Active pull request: #46 (`M2: add multi-deck key-lock transition isolation coverage`).
- Previous exact-head checkpoint `14888e25c50a9a9400acbf45315366198643b31f` passed Build and test run `35507483163`: Linux ASan/UBSan, generated-progress and full configured CTest plus Windows x64 configure/build/full CTest/audio diagnostics/native GUI lifecycle smoke/silent device probe/staging/artifact upload all succeeded.
- The current package extends that same PR with a deterministic no-audio native resize-smoke contract and CI artifact; a fresh exact-head run is required before integration.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA).
- GitHub Releases is empty; no public BrokeDJ Release exists or is qualified by this checkpoint.

## Current PR #46: four-deck transition and native GUI hardening

1. **Four-deck optional key-lock isolation**
   - The lifecycle fixture submits and stages four independent immutable clips and requires every optional renderer to accept the initial callback window.
   - An unannounced rate change, invalid pitch, disable/re-enable cycle or device transition on one owner remains deck-local while unaffected owners continue rendering.

2. **Deterministic Windows native resize smoke**
   - `--smoke-test` keeps audio-device initialization disabled and now drives the real native window through 1050x800, 1280x860, 1600x900 and back to 1050x800.
   - Each step checks that the requested top-level geometry is accepted and that the owned `MainComponent` retains a positive content area; failure exits non-zero instead of silently timing out later.
   - The app writes schema-versioned `BrokeDJ-gui-smoke.json` with requested/observed geometry, `plays_audio=false`, `opens_audio_device=false`, a success flag and an explicit qualification limitation.

3. **CI verifies and preserves the smoke evidence**
   - Windows CI parses the JSON contract, requires all four resize steps, rejects geometry mismatches and copies the report into the development artifact as `GUI-SMOKE.json`.
   - This removes a blind spot in the previous launch-only smoke without pretending that automation can judge visual overlap, HiDPI rendering or real user interaction.

## Validation evidence and gates still open

- The previous PR #46 exact-head `14888e25c50a9a9400acbf45315366198643b31f` is green in run `35507483163`; the new resize-smoke package must pass a fresh exact-head Linux + Windows gate before merge.
- Real Windows 11 clean-machine/manual resize/HiDPI/device-switching validation remains open; the automated resize sequence is a crash/geometry contract, not a human visual review.
- Independent master 1/2 and cue 3/4 still require a real four-output interface and listening verification.
- Physical slow-storage, multi-minute real-world compressed files and hardware underrun behavior remain open; controlled decoder fixtures are not physical-storage qualification.
- Native MIDI/controller mappings and concrete controller profiles remain unqualified; the current native jog input is mouse/native-GUI only.
- Representative music-domain BPM/key/grid evidence, production key-lock listening/latency, wider scratch/controller evidence, multi-hour soak and public-release qualification remain open.

## Documentation drift found during this audit

`README.md`, `ROADMAP.md` and `docs/ARCHITECTURE.md` still contain older wording saying later variable-tempo segment controls are not exposed and/or that Slip/Reverse/Scratch are wholly open. The current source already contains `TempoSegmentEditorComponent`, native reviewed-grid add/move/replace/remove plus undo/redo wiring, four-deck REV/SLIP controls and the bounded native JOG/SCRATCH strip. Those documents still need a truthful synchronization pass without changing `docs/progress.json` or claiming production key-lock/controller/hardware qualification.

## Next largest step

Fix any exact-head regression from the new resize-smoke contract first. If green, keep PR #46 open until the normal integration window or a larger coherent M2 slice justifies merge; then synchronize stale M2 documentation and continue representative analysis evidence plus key-lock/controller transition qualification while preserving the manual M1 hardware gates.
