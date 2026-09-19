# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness or live-performance claim.

## Current checkpoint

- Default branch: `main`.
- Verified `main` baseline before this package: `e90196c8e4c604f5546163a3714a2df600105442`; push CI run `35435274776` completed successfully.
- Active branch: `fix/rate-aware-sync`.
- Pull request: #34, `M2: make one-shot Sync rate-aware` — open at this checkpoint and not yet merged.
- Implementation head before this documentation checkpoint: `26945a988fbc8d837aed0a0ea4351a3563754369`.
- Implementation CI: run `35448320980`; Linux sanitizer/core job completed successfully while the Windows x64 development job was still running when this checkpoint text was prepared. This documentation commit creates a newer PR head and therefore requires its own exact-head CI before merge.
- Roadmap source of truth remains `docs/progress.json`: 1/10 milestones complete (10.0%, PRE-ALPHA).
- No public BrokeDJ Release exists or is qualified by this checkpoint.

## Implemented in PR #34

1. **Rate-aware one-shot Sync correctness**
   - Reviewed-grid Sync now targets the master's effective tempo (`grid BPM × current master playback rate`) instead of raw source BPM alone.
   - Master rate is sampled exactly once on the control side for each command; beat phase still comes from the master's source-grid position.
   - The follower receives an absolute bounded transport-rate command while ordinary Engine smoothing/de-clicking remains responsible for the resulting control transition.

2. **Fail-closed rate envelope and diagnostics**
   - Non-finite/out-of-range master rates are rejected before follower controls mutate.
   - The existing caller rate envelope remains authoritative; float boundary noise is tolerated only at the numeric edge and the accepted command is clamped back to that exact envelope.
   - `BeatSyncPlan` records master playback rate and effective BPM for deterministic diagnostics without adding callback logging or I/O.

3. **Deterministic regression coverage and truthful docs**
   - Added tests for slowed 0.8x and accelerated 1.2x master tempo, inclusive safety-boundary behavior, out-of-envelope rejection, non-finite-rate rejection and transactional no-drift behavior.
   - README/roadmap wording is corrected to reflect already-merged reviewed-grid beat loops and persistent Hot Cue controls while keeping normal Beat Jump/Sync UI, representative-music validation and hardware qualification explicitly open.

## Verification

- Exact implementation head `26945a988fbc8d837aed0a0ea4351a3563754369`: GitHub Actions run `35448320980`.
- Linux `ubuntu-24.04`: generated-progress check, sanitizer/core configure/build and full configured CTest — success at checkpoint time.
- Windows `windows-2022`: configure succeeded and the development build was still running at checkpoint time.
- The final documentation head must pass a fresh exact-head Linux + Windows run before merge; do not treat the partial checkpoint above as the merge gate.

The Windows CI smoke does not replace physical audio-interface, multi-output cue, controller, clean-machine listening or soak qualification.

## Gates still open

- Exact-final-head CI for PR #34 before merge.
- M1 physical Windows 11 audio-device qualification, including verified four-output cue routing where the interface supports it.
- Representative legal local-music corpus validation for BPM/key/grid behavior.
- Real listening/soak testing, controller testing and production release qualification.
- Normal Beat Jump controls and master/follower Sync UI/policy; continuous phase lock remains a separate later feature.
- Full tempo-segment editing, slip/reverse/scratch and broader M2 workflow completion.

## Next largest step

After PR #34 is exact-head green and merged, expose compact Beat Jump plus an explicit master/follower one-shot Sync control in the native deck UI. The UI must reflect programmatic follower-rate changes without feedback loops, retain the reviewed-grid/fail-closed owner boundary and keep continuous phase lock out of scope until separate timing/device evidence exists.
