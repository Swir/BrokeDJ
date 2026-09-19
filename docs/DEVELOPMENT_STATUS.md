# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness or live-performance claim.

## Current checkpoint

- Default branch: `main` at merged PR #35 checkpoint `62d2bd110442090c3507dd3dd1251bf54364b6e6`.
- PR #35 (`M2: expose Beat Jump and one-shot Sync UI`) final head `9546182ff440ed748ef40bd3a2336c6480bff403` passed exact-head GitHub Actions run `35464388653` and was merged to `main`.
- Active development branch / PR: `feat/m1-device-probe` / PR #36 (`M1: add silent audio-device witness probe`).
- The first PR #36 checkpoint `3a34adc432cee8b74d61c6875526d64ef04c12f8` passed exact-head GitHub Actions run `35465256099`.
- The current package extends that green baseline with a schema-versioned machine-readable probe report, explicit no-open safety invariants and stronger Windows CI validation. The containing branch head requires a fresh exact-head run before integration.
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
   - `BrokeDJ.exe --device-probe` writes both `BrokeDJ-device-probe.txt` and schema-versioned `BrokeDJ-device-probe.json`.
   - The full local probe constructs JUCE device descriptors for advertised capabilities but never calls `AudioIODevice::open`, starts no callback and emits no sound.
   - It checks descriptor `isOpen()` state before and after capability queries and returns non-zero if the non-opening invariant is violated.
   - Four-output descriptors remain capability candidates only; they are never treated as proof of physical cue routing.

2. **Machine-readable CI contract**
   - `--device-probe-ci` remains enumeration-only and does not create per-device descriptors.
   - Windows CI parses the JSON contract, verifies schema/safety booleans, checks serialized backend counts and rejects any CI-mode descriptor entries.
   - TXT and JSON probe evidence are retained alongside the development artifact diagnostics.

3. **Manual witness procedure**
   - `docs/M1_HARDWARE_WITNESS.md` defines clean launch, resize/HiDPI, import/playback, device switching and physical outputs 1/2 versus cue 3/4 evidence.
   - The witness records the probe schema/checksum without requiring private device inventories, file paths or music to be committed.

## Gates still open

- Fresh exact-head Linux/Windows CI for the latest PR #36 branch checkpoint before integration.
- Real Windows 11 clean-machine/audio-interface validation, including actual device switching and independent four-output cue where supported.
- Manual resize/HiDPI usability review of the denser performance-control layout.
- Representative legal local-music corpus validation for BPM/key/grid behavior.
- Real listening/soak, controller and production-release qualification.
- Full tempo-segment editing, production key lock, slip/reverse/scratch and broader M2 workflow completion.

## Next largest step

First require the latest PR #36 head to pass its exact-head Linux/Windows gate. Because physical M1 qualification still needs real hardware, the next implementation package should then advance M2 rather than repeatedly re-auditing the same hardware blocker: prioritize complete tempo-segment editing and persistence for reviewed variable-tempo grids, followed by representative corpus validation, while keeping ordinary playback independent of analysis.
