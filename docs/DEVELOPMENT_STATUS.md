# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch: `main` at `45864d24edba862e7af794548e9aa907f82defeb` after PR #46 (`M1/M2: harden four-deck transitions and native resize smoke`).
- Active development branch: `feat/m2-continuous-sync-lock`.
- Active pull request: #47 (`M2: add bounded continuous reviewed-grid Sync lock`).
- Previous exact-head `0b14405c3ac73c9664064b466e95170a703115e5` passed Build and test run `35513887685` across Linux sanitizers/progress/full CTest and Windows x64 build/full CTest/audio diagnostics/native resize smoke/silent device probe/staging.
- Sync ownership hardening was applied as `8c53cff959e696dff865405166f2d58a2bd015bb`; its bot-authored pull-request event produced run `35514640842` with `action_required` and no jobs, so this owner-authored checkpoint intentionally starts a fresh exact-head required run before merge.
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

3. **Manual transport ownership and variable-tempo regression hardening**
   - Explicit follower rate/loop/seek, Hot Cue and Beat Jump actions release that follower's lock instead of being silently overwritten by the next maintenance tick.
   - Incompatible Beat Loop or Reverse/Slip ownership on the MASTER releases all followers immediately; manual master-rate changes intentionally remain trackable through effective-tempo re-evaluation.
   - Invalid MASTER selection preserves the already-valid master/follower relationship instead of destructively clearing it.
   - Core regression coverage crosses reviewed variable-tempo boundaries and verifies follower/master clip replacement invalidates stale grid intent without drifting rate/seek controls.

## Gates still open

- A fresh exact-head Linux sanitizer + Windows x64 native build/full CTest/audio diagnostics/no-audio GUI smoke/device probe/staging run is required for the final PR head before merge.
- M1 still requires real Windows 11 clean-machine/manual resize/HiDPI/import/device-switching checks and real four-output master 1/2 versus cue 3/4 verification.
- Representative user-owned/licensed music-domain BPM/key/grid evidence remains open.
- Production key-lock listening/latency, MIDI/controller mappings and concrete controller profiles remain unqualified.
- Physical storage/underrun behavior, multi-hour soak and public alpha/beta Release qualification remain open.

## Next largest step

Run the fresh exact-head Linux sanitizer + Windows x64 gate for this hardened Sync slice. If green, integrate the coherent PR; then prioritize representative analysis evidence and the remaining M2 key-lock/controller qualification without weakening the manual M1 hardware gates.
