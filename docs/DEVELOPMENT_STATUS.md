# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target. The immediate UX objective is a complete, user-testable four-deck workstation surface rather than more unrelated framework work.

## Current checkpoint

- Default branch baseline is `main` at `bcf4615b2d3a3349652bac343c4c57be254451bb`, the squash merge of PR #103 (`Add guided first-Beta witness runner`). PR #103 exact head `bb7d7d4f59ee23f28a349101d24dcf44290ccd08` passed Build/test #528, Native library scale #198 and all M1/M2/M3/Beta/witness-runner tool workflows before merge.
- Build/test #528 published a smoke-qualified **BrokeDJ Beta Preview Windows x64** workflow artifact bound to exact head `bb7d7d4f...`. Its portable ZIP passed manifest/SHA-256 validation, relocated extraction with spaces/non-ASCII text, native no-audio GUI lifecycle/resize smoke and silent device-probe smoke. It remains a development/Beta Preview artifact, not a public Beta Release or live qualification.
- Active package is draft PR #104, branch `ux/beta-workstation-full-frame`. The implementation head before this checkpoint is `754e9425ad8b8f395c696346f56e18182ba7ab9b`; this checkpoint commit follows it, so **the newest PR head and its exact-head workflows are the only merge authority**.
- PR #104 addresses a concrete visual-Beta defect in evidence rather than accepting a misleading screenshot: an earlier 1600-class Windows witness could sample the old 1280-wide frame immediately after resize and leave a large black strip on the right while still passing global palette metrics.
- The visual collector now waits for a stable native size, requests pending window paint, flushes DWM composition and only then captures compact/workstation frames. The manifest records the dwell and explicit settled-frame markers.
- The pixel gate now independently measures whole-frame, right-quarter and bottom-quarter painted coverage. A deterministic self-test recreates the partial-width black-strip regression and requires rejection, while preserving the blank/theme/palette checks.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. This is the whole M0–M9 roadmap count, not first-Beta readiness, audio quality or time remaining. No milestone is being marked complete from screenshot/tooling work.
- GitHub Releases remains empty; no public Beta/Stable Release or live-performance qualification is claimed.

## Verified implementation state

- **M1:** native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging, native four-deck async import/replacement isolation, deterministic device-reprepare transport continuity, conservative loss pause/no-auto-resume behavior, `AudioIODevice::isOpen()` loss detection and fast open-device A-to-B replacement fail-safe are implemented. The remaining gate is real Windows 11 clean-machine switching/loss plus physical four-output master/cue isolation.
- **M2:** reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle are implemented. Native `MainComponent`/DeckPanel integration covers PLAY/rate/LOOP/CUE 0, live seek and fail-closed clip replacement. Representative real-music BPM/key review, key-lock listening/latency qualification and physical controller workflows remain open.
- **M3:** channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests are implemented. Physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- **M4:** SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery, native async session/adoption coverage and privacy-safe two-pass fixture-state verification are integrated. The connected witness is schema-2 and exact-process-bound; the actual human Windows 11 run remains required.
- **First-Beta operator path:** PR #103 is merged. `scripts/beta_witness_runner.ps1` coordinates the canonical M1–M4 witnesses around one exact executable/evidence directory, reuses only evidence that validates, keeps M2/M3 playback/microphone/recording human-controlled, refuses CI evidence generation and composes the canonical qualification only after all four milestone witnesses pass.
- **Packaging:** portable Beta Preview bytes are bound to the staged tree and exact source identity; the extracted executable is smoke-tested from a relocated Windows path before publication as a workflow artifact.
- **Workstation UI:** four deck panels, central four-channel mixer at workstation sizes, library/history launchers, recording/microphone/booth/limiter controls, responsive compact layout and PL/EN system-language selection are implemented. PR #104 is specifically tightening the Windows pixel evidence so a transient partial-width frame can no longer masquerade as the fully presented workstation.

## Unmet gates

- **PR #104:** exact-head Build/test, Native library scale and UI visual witness must all pass on the newest branch head. The settled-frame screenshot must satisfy the stricter full-surface coverage gate; any failure is a regression signal to fix, not a threshold to weaken merely to obtain green CI.
- **M1:** real Windows 11 clean-machine launch/resize/import/playback, live device replacement/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification. Native synthetic integration tests are not substitutes for those human/device gates.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** perform and review the user-controlled Windows 11 connected library/session witness against an exact staged executable. Exact-process ownership does not fabricate launch/resize/import/search/tag/playlist/history/duplicate/missing/relocate/backup/restore/session evidence.
- **Release:** no public Beta/Stable Release until the documented gate for that scope is satisfied. A workflow Beta Preview is intentionally user-testable but remains distinct from a qualified GitHub Release.

## Next largest step

Finish PR #104 on exact-head green and inspect the resulting **settled 1600-class workstation screenshot** rather than relying on geometry JSON alone. If the full-surface gate exposes an actual application repaint/layout defect, repair the app before merge. Once this UI package is integrated, use the existing smoke-qualified Beta Preview plus the guided runner on a real Windows 11 x64 machine to close M4 connected-library/session evidence, then the remaining physical M1 and listening/hardware M2–M3 gates. Do not widen into M5+ until the first usable Beta gate is closed or an M5 item is required to remove a Beta blocker.
