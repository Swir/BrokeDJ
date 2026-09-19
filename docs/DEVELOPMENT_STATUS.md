# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness or live-performance claim.

## Current checkpoint

- Default branch: `main`.
- Verified `main` baseline before this package: `9f7ca0c981951edbc7865ad334c4e38ca9b99e2a`; push CI run `35448924458` completed successfully.
- Active branch: `feat/beat-jump-sync-ui`.
- Pull request: #35, `M2: expose Beat Jump and one-shot Sync UI` — open and not merged at this checkpoint.
- Implementation head: `e6f2e408c037443820b9bee50333c86c2e5802d7`.
- Implementation CI: run `35463896466` completed successfully for Linux sanitizer/core checks and the Windows x64 development gate, including configure/build, full configured CTest, audio diagnostics, native no-audio GUI lifecycle smoke, staging and artifact upload.
- This documentation commit creates a newer PR head and therefore requires its own exact-head CI before merge.
- Roadmap source of truth remains `docs/progress.json`: 1/10 milestones complete (10.0%, PRE-ALPHA).
- No public BrokeDJ Release exists or is qualified by this checkpoint.

## Implemented in PR #35

1. **Native reviewed-grid Beat Jump controls**
   - Each deck exposes backward/forward Beat Jump with 1/2/4/8/16-beat distances.
   - The UI delegates to the existing JUCE-independent `PerformanceDeckOwner::jumpBeatsFromTransport` path, preserving fractional beat phase and the owner boundary.
   - Missing/invalid grids, unavailable transport and out-of-track targets fail closed and surface a user-visible status instead of mutating transport unpredictably.

2. **Explicit bounded one-shot MASTER / SYNC workflow**
   - One loaded reviewed-grid deck can be selected as the explicit Sync master.
   - Other reviewed-grid decks expose one-shot SYNC through the already-tested rate-aware owner planner; this remains bounded tempo/phase alignment, not continuous phase lock.
   - Replacing the selected master clip clears that master designation so source-identity changes cannot silently retain stale master intent.

3. **Rate-control coherence and optional key-lock fail-closed behavior**
   - The native Rate knob now reflects programmatic Engine rate changes with `dontSendNotification`, avoiding a second GUI-to-Engine rate command and feedback loops after Sync.
   - Successful Beat Jump reports the normalized seek discontinuity to the optional key-lock research lifecycle.
   - Successful Sync reports a transport-control discontinuity to that lifecycle so the optional renderer disarms and ordinary production playback remains the immediate fallback until safe restaging.

## Verification

- Exact implementation head `e6f2e408c037443820b9bee50333c86c2e5802d7`: GitHub Actions run `35463896466` — success.
- Linux `ubuntu-24.04`: generated-progress check, sanitizer/core configure/build and full configured CTest — success.
- Windows `windows-2022`: configure/build, full configured CTest, audio render/callback diagnostics, native no-audio GUI lifecycle smoke, staging and artifact upload — success.
- The final documentation head created after this checkpoint text must pass a fresh exact-head Linux + Windows run before merge.

The Windows CI smoke does not replace physical audio-interface, multi-output cue, controller, clean-machine listening, resize/usability review or soak qualification.

## Gates still open

- Exact-final-head CI for PR #35 before merge.
- M1 physical Windows 11 audio-device qualification, including verified four-output cue routing where the interface supports it.
- Manual resize/usability review of the denser performance-control deck layout at the documented window limits and HiDPI scaling.
- Representative legal local-music corpus validation for BPM/key/grid behavior.
- Real listening/soak testing, controller testing and production release qualification.
- Continuous phase lock remains a separate later feature; this PR implements only bounded one-shot Sync.
- Full tempo-segment editing, production key lock, slip/reverse/scratch and broader M2 workflow completion.

## Next largest step

After PR #35 is exact-final-head green, inspect the final patch and merge only if the required checks still match that head. Then perform the next M2 hardening package: responsive/native layout validation for the denser deck controls, broader Beat Jump/Sync lifecycle tests, and the next safe performance control without weakening ordinary playback or the physical-device gates.
