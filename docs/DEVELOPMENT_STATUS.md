# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope is frozen around completing and qualifying M1–M4. M5+ work remains later unless it directly removes a blocker for this target.

## Current checkpoint

- Default branch baseline: `main` at `f6a2255df49b902d8ea4948898b3c0f4df110575`, the squash merge of PR #88. Exact PR head `cf06959b49f9e7ff656184460318e08281b13970` passed Beta qualification host compatibility #3, Beta qualification tool #20, Native library scale smoke #120 and Build and test #450 before integration.
- Active development package: `feat/m4-repro-witness-fixture`, draft PR #89. The package keeps the M4 evidence schema and human acceptance boundary unchanged while removing the need to use personal music during the remaining connected-library/session witness.
- Implementation checkpoint `5aa5151f4f665a82806ba54dabe956926cfdd313` adds a disposable generated 48 kHz stereo 16-bit PCM WAV, a byte-identical duplicate and an empty relocation directory. The witness prints those local-only paths for the operator, never stores them in evidence, never starts playback and removes the workspace when the run ends.
- PR #89 also adds a fixture-only CI path that validates RIFF/WAVE structure, expected PCM parameters, duplicate byte identity and cleanup without minting human evidence. The same self-test is required under PowerShell 7 and built-in Windows PowerShell 5.1.
- This status/docs checkpoint advances the branch head after the implementation commit, so the exact final-head M4 witness-tool, Build/Test and relevant native/package checks must run again before any integration. No M4 completion or Beta readiness is inferred from fixture-tool tests.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 remain open until their documented acceptance evidence exists.
- GitHub Releases remains empty; no public Beta/Release or live-performance qualification is claimed.

## Verified implementation state

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, a privacy-safe hardware witness recorder, portable Beta Preview packaging and native four-deck async import/replacement isolation. Real clean-machine import/playback, device switching and physical master 1/2 versus CUE 3/4 isolation remain manual.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle. Representative real-music BPM/key evidence, listening/latency qualification and physical controller workflows remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests. PR #87 is integrated and hardens recorder serialization, reason-specific integrity evidence, the callback heap contract and the requested FIFO-capacity contract; physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and native async session/adoption round-trip coverage. PR #89 removes personal-track preparation from the remaining witness but does not replace the documented user-controlled connected Windows 11 workflow.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. PR #88 is integrated and provides PowerShell 5.1 host compatibility, unattended-generation refusal and atomic summary publication; it cannot replace human hardware/listening workflows and does not authorize a public Beta by itself.

## Unmet gates

- **M1:** real Windows 11 clean-machine launch/resize/import/playback, device-switch/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** reviewed connected Windows 11 library/session witness against the exact staged executable.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied.

## Next largest step

Keep PR #89 on its development branch until the exact final head passes the M4 witness-tool workflow plus repository Build/Test and relevant native/package checks; repair any regression before integration. Then keep the Beta scope frozen and run/review the privacy-safe M4 connected-library/session witness on Windows 11 x64 using the generated disposable fixture workspace against one exact staged executable. Fix any regression it exposes before changing milestone status, then complete the remaining M1–M3 hardware/listening evidence. Do not widen the target into M5+ merely to keep development busy.
