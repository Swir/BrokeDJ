# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness or live-performance claim.

## Current checkpoint

- Default branch: `main`.
- PR #35 (`M2: expose Beat Jump and one-shot Sync UI`) final head `9546182ff440ed748ef40bd3a2336c6480bff403` passed exact-head GitHub Actions run `35464388653` and was merged to `main` as `62d2bd110442090c3507dd3dd1251bf54364b6e6`.
- Post-merge `main` run `35464822029` started for `62d2bd110442090c3507dd3dd1251bf54364b6e6`; it was still running when this development checkpoint was prepared, so this file does not claim that post-merge push validation is complete.
- Active development branch: `feat/m1-device-probe`.
- This branch adds a silent/non-opening device capability probe and an M1 manual witness procedure, while correcting README wording after PR #35. It requires its own exact-head CI before any later integration.
- Roadmap source of truth remains `docs/progress.json`: 1/10 milestones complete (10.0%, PRE-ALPHA).
- No public BrokeDJ Release exists or is qualified by this checkpoint.

## Newly integrated on main: PR #35

1. **Native reviewed-grid Beat Jump controls**
   - Each deck exposes backward/forward Beat Jump with 1/2/4/8/16-beat distances.
   - The UI delegates to the JUCE-independent `PerformanceDeckOwner::jumpBeatsFromTransport` path, preserving fractional beat phase and fail-closed transport ownership.

2. **Explicit bounded one-shot MASTER / SYNC workflow**
   - One loaded reviewed-grid deck can be selected as the explicit Sync master.
   - Other reviewed-grid decks expose one-shot SYNC through the tested rate-aware owner planner; this remains bounded tempo/phase alignment, not continuous phase lock.
   - Replacing the selected master clip clears stale master intent.

3. **Rate-control/key-lock lifecycle coherence**
   - Programmatic Sync rate changes are reflected in the Rate control with `dontSendNotification`, avoiding a GUI feedback command.
   - Beat Jump/Sync transport changes notify the optional research key-lock lifecycle so it fails closed to ordinary playback until safe restaging.

## Current development package: M1 hardware witness tooling

1. **Silent device capability probe**
   - `BrokeDJ.exe --device-probe` scans JUCE output-device descriptors and writes `BrokeDJ-device-probe.txt` in the current working directory.
   - The probe does not open an audio device, start an audio callback or emit sound.
   - It records advertised backend/device/output-channel/sample-rate/buffer-size capabilities and marks descriptors with at least four output channels only as candidates, never as verified cue routing.

2. **CI path hardening**
   - `--device-probe-ci` performs enumeration-only discovery on the Windows runner.
   - The workflow verifies the report exists and explicitly states `plays_audio=no` and `opens_device=no`, then retains the CI report with development diagnostics.

3. **Manual witness procedure**
   - `docs/M1_HARDWARE_WITNESS.md` defines clean launch, resize/HiDPI, import/playback, device switching and physical outputs 1/2 versus cue 3/4 evidence without pretending hosted CI can supply hardware qualification.

## Gates still open

- Exact-head CI for `feat/m1-device-probe` before integration.
- Completion of post-merge `main` run `35464822029` for `62d2bd110442090c3507dd3dd1251bf54364b6e6`.
- Real Windows 11 clean-machine/audio-interface validation, including actual device switching and independent four-output cue where supported.
- Manual resize/HiDPI usability review of the denser performance-control layout.
- Representative legal local-music corpus validation for BPM/key/grid behavior.
- Real listening/soak, controller and production-release qualification.
- Full tempo-segment editing, production key lock, slip/reverse/scratch and broader M2 workflow completion.

## Next largest step

First get this M1 witness/tooling branch exact-head green. The next implementation package should then use the new witness path to reduce the physical-device evidence gap when hardware is available, while continuing M2 with the highest-value safe deck feature (full tempo-segment editing or another reviewed-grid performance control) without weakening ordinary playback or release gates.
