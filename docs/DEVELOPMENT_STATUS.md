# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch: `main`; PR #47 (`M2: add bounded continuous reviewed-grid Sync lock`) was merged as `a01d3b33f1c93e87c1f88058e3ae3a6daaabdca6`.
- Integration source head: `d9a424ab3b7ee9dea2f591c529855fff1c0bee9c` from `feat/m2-continuous-sync-lock`.
- Exact-head Build and test run `35514697364` passed for that integration head: Linux generated-progress + ASan/UBSan/full CTest and Windows x64 configure/build/full CTest/audio diagnostics/native no-audio lifecycle+resize smoke/silent device probe/staging/artifact upload all succeeded.
- PR #47 is merged; there is no active BrokeDJ development PR at this checkpoint.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA).
- GitHub Releases remains empty; no public BrokeDJ Release exists or is qualified by this checkpoint.

## Integrated M2 slice: bounded continuous reviewed-grid Sync

1. **Continuous owner-side maintenance without realtime callback expansion**
   - `PerformanceDeckOwner` exposes bounded maintenance for an established reviewed-grid Sync relationship.
   - Master effective tempo is re-evaluated outside the audio callback; follower rate writes use an epsilon, phase seeks are suppressed inside an 0.08-beat deadband and phase corrections beyond 0.35 beat fail closed.
   - Reviewed Beat Loop, Reverse or Slip ownership blocks maintenance rather than allowing transport workflows to fight each other.

2. **Native PL/EN follower lock workflow**
   - SYNC is a real toggle after bounded initial alignment, serviced at 5 Hz from the message thread while master/follower playback is active.
   - Native Jog/Scratch temporarily suspends maintenance while the platter is touched; Beat Loop or Reverse/Slip releases the relevant lock.
   - Master replacement/clear releases followers and follower clip replacement clears only that follower. Optional key-lock research is notified only for accepted rate/phase maintenance changes.

3. **Manual transport ownership and variable-tempo regression hardening**
   - Explicit follower rate/loop/seek, Hot Cue and Beat Jump actions release that follower's lock instead of being silently overwritten by the next maintenance tick.
   - Incompatible Beat Loop or Reverse/Slip ownership on the MASTER releases all followers immediately; manual master-rate changes intentionally remain trackable through effective-tempo re-evaluation.
   - Invalid MASTER selection preserves an already-valid master/follower relationship instead of destructively clearing it.
   - Core regression coverage crosses reviewed variable-tempo boundaries and verifies follower/master clip replacement invalidates stale grid intent without drifting rate/seek controls.

## Gates still open

- M1 still requires real Windows 11 clean-machine/manual resize/HiDPI/import/device-switching checks and real four-output master 1/2 versus cue 3/4 verification.
- Representative user-owned/licensed music-domain BPM/key/grid evidence remains open.
- Production key-lock listening/latency, MIDI/controller mappings and concrete controller profiles remain unqualified.
- Physical storage/underrun behavior, multi-hour soak and public alpha/beta Release qualification remain open.

## Next largest step

Prioritize representative analysis evidence and the remaining M2 key-lock/controller qualification while preserving the manual M1 hardware gates. Any next substantial implementation slice should start from fresh `main` and use a new feature branch/PR.
