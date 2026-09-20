# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch: `main` at `45864d24edba862e7af794548e9aa907f82defeb` after PR #46 (`M1/M2: harden four-deck transitions and native resize smoke`).
- Active development branch: `feat/m2-continuous-sync-lock`.
- Active pull request: #47 (`M2: add bounded continuous reviewed-grid Sync lock`).
- Implementation head before this status-only checkpoint: `bc8c8f386f46d85e97fa9001ff58d91ff058a5e6`.
- Exact-head Build and test run `35513860812` started for that implementation head; this checkpoint commit intentionally requires a fresh exact-head PR run before merge.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA).
- GitHub Releases remains empty; no public BrokeDJ Release exists or is qualified by this checkpoint.

## Current PR #47: bounded continuous reviewed-grid Sync

1. **Continuous owner-side maintenance without realtime callback expansion**
   - `PerformanceDeckOwner` now exposes bounded maintenance for an already-established reviewed-grid Sync relationship.
   - Master effective tempo is re-evaluated outside the audio callback; follower rate writes use an epsilon, phase seeks are suppressed inside an 0.08-beat deadband and phase corrections beyond 0.35 beat fail closed.
   - Reviewed Beat Loop, Reverse or Slip ownership on either deck blocks maintenance rather than allowing two transport workflows to fight each other.

2. **Native PL/EN follower lock workflow**
   - SYNC is a real toggle after bounded initial alignment, serviced at 5 Hz from the message thread while master/follower playback is active.
   - Native Jog/Scratch temporarily suspends maintenance while the platter is touched; Beat Loop or Reverse/Slip releases that follower lock.
   - Master replacement/clear releases followers and follower clip replacement clears only that follower. Optional key-lock research is notified only for accepted rate/phase maintenance changes.

3. **Deterministic coverage and truthful documentation**
   - Dependency-free Release core CTest passed 12/12 locally from the PR #46 source artifact plus this patch.
   - Selected ASan/UBSan tests passed 4/4: performance deck owner, tempo-segment editor model, jog/scratch controller and reverse/slip stream stress.
   - Transfer workflow run `35513788334` applied the exact verified patch and reran dependency-free core tests plus `scripts/update_progress.py --check` successfully.
   - README, ROADMAP, ARCHITECTURE, TESTING and CHANGELOG now describe the already-integrated native variable-tempo editor, REV/SLIP, native JOG/SCRATCH and the new bounded continuous Sync without changing milestone completion.

## Gates still open

- A fresh exact-head Linux sanitizer + Windows x64 native build/full CTest/audio diagnostics/no-audio GUI smoke/device probe/staging run is required for the final PR head before merge.
- M1 still requires real Windows 11 clean-machine/manual resize/HiDPI/import/device-switching checks and real four-output master 1/2 versus cue 3/4 verification.
- Representative user-owned/licensed music-domain BPM/key/grid evidence remains open.
- Production key-lock listening/latency, MIDI/controller mappings and concrete controller profiles remain unqualified.
- Physical storage/underrun behavior, multi-hour soak and public alpha/beta Release qualification remain open.

## Next largest step

Fix any exact-head PR regression first. If the final PR head passes Linux and Windows required controls, integrate this coherent Sync slice; then prioritize representative analysis evidence and controller/key-lock qualification while preserving the manual M1 hardware gates.
