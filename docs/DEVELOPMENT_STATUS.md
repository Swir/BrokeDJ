# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope is frozen around completing and qualifying M1–M4. M5+ work remains later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline: `main` at `c884c535b26ab410fff9f8926d5ef2984c7375c5`. Its push-main Build and test #445 and Native library scale smoke #115 both passed after PR #87 integration.
- Active development package: `fix/beta-qualification-host-atomicity`, draft PR #88. The package keeps the Beta evidence criteria unchanged while removing clean-host friction and stale-output risk from the exact-executable M1–M4 qualification orchestrator.
- `scripts/beta_qualification.ps1` now launches every witness validator with the same PowerShell host that launched the orchestrator instead of requiring `pwsh` unconditionally. This keeps PowerShell 7 support while allowing the built-in Windows PowerShell 5.1 path on a clean Windows 11 machine.
- Human Beta-summary generation now rejects common truthy CI forms (`1`, `true`, `yes`, `on`) before resolving or hashing the supplied application/evidence files. Existing-summary validation remains permitted in CI because it creates no new human evidence.
- New Beta qualification output is written to a sibling temporary file, fully revalidated against the exact executable/source/evidence fingerprints, then moved into place. Failure cleans the temporary file and does not publish a partial new qualification summary.
- Exact-head CI for PR #88 is required before integration; no Beta/hardware/listening evidence is inferred from the automation contract tests.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 remain open until their documented acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, a privacy-safe hardware witness recorder, portable Beta Preview packaging and native four-deck async import/replacement isolation. Real clean-machine import/playback, device switching and physical master 1/2 versus CUE 3/4 isolation remain manual.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle. Representative real-music BPM/key evidence, listening/latency qualification and physical controller workflows remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests. PR #87 is integrated and hardens recorder serialization, reason-specific integrity evidence, the callback heap contract and the requested FIFO-capacity contract; physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and native async session/adoption round-trip coverage. The remaining M4 gate is the documented user-controlled connected-library/session witness on Windows 11 x64.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. PR #88 hardens host compatibility, unattended-generation refusal and atomic summary publication; it cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **M1:** real Windows 11 clean-machine launch/resize/import/playback, device-switch/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** reviewed connected Windows 11 library/session witness against the exact staged executable.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Keep PR #88 on its development branch until the exact final head passes the relevant Windows contract/build/package checks; repair any regression before integration. Then keep the Beta scope frozen and run/review the privacy-safe M4 connected-library/session witness on Windows 11 x64 against one exact staged executable. Fix any regression it exposes before changing milestone status, then complete the remaining M1–M3 hardware/listening evidence. Do not widen the target into M5+ merely to keep development busy.
