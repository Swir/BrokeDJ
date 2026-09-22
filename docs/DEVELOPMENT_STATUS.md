# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying the already-implemented M1–M4 core workflow before unrelated feature expansion. Optional M5–M8 breadth remains later work unless a concrete Beta blocker requires it.

## Current checkpoint

- Default branch baseline: `main` at `aadfca0630494e6d60c4c7dc9cc91c87e96d73e8`, the squash merge of PR #75 (`Add exact-executable first Beta qualification gate`).
- GitHub Releases remains empty. Active development continues in draft PR #76 on branch `feat/pro-ui-pass`; no competing open BrokeDJ issue/PR was present at the start of this checkpoint.
- The UI branch rebuilds the normal-size four-deck workspace around a dedicated central mixer while retaining the established compact layout below the workstation breakpoint. Existing Engine callbacks and audio/DSP ownership are unchanged by the UI reparenting/layout work.
- Exact-head `629edb0cbac66ca88c021de6bf5dc2cc6b0fa721` passed Build and test `35726719754`, Native library scale smoke `35726719617`, M1 hardware witness `35726719675`, M2 key-lock witness `35726719624`, M3 mixer/recording witness `35726719637` and Beta qualification tool `35726719695`.
- UI visual witness run `35726719727` failed after the application built and the hosted runner observed/captured compact/workstation-class windows. The failure was in PowerShell witness bookkeeping (`Argument types do not match`), not in BrokeDJ compilation or the native resize lifecycle itself.
- This checkpoint replaces the generic PowerShell `List[object]`/`HashSet[string]` witness bookkeeping with simple arrays/hashtables to avoid the hosted PowerShell conversion failure. It also preserves failed-capture diagnostics as a short-lived workflow artifact, so a future pixel-capture regression remains inspectable instead of disappearing after a failed step.
- Fresh exact-head CI is required after this checkpoint. Do not merge PR #76 until the updated UI visual witness and the other required exact-head workflows are green.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. UI restructuring, polish and qualification tooling do not advance the milestone counter by themselves.

## Integrated foundations on main

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, a silent device-capability probe and a packaged privacy-safe hardware witness recorder. Real clean-machine playback, device switching and physical master 1/2 versus CUE 3/4 isolation remain manual.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH, the opt-in Signalsmith-backed key-lock research lifecycle and a packaged privacy-safe listening witness recorder. Representative music-domain and real listening/latency/hardware evidence remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, opt-in microphone ducking, limiter measurements, deterministic EQ qualification and a packaged mixer/recording witness. Physical/listening gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe backup/restore and native 5k-track/12k-history staged-EXE recovery.
- The merged Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace those human hardware/listening workflows and does not make a public Beta Release by itself.

## Gates still open before first Beta qualification

- M1: real Windows 11 clean launch/resize/import/playback/device-switch/four-output-cue witness on actual hardware.
- M2: representative music-domain BPM/key evidence, real key-lock listening evidence, device CPU/callback-deadline/underrun and latency qualification, plus controller/wider scratch qualification required by the current milestone wording.
- M3: real Windows 11 reviewed EQ/mixer listening plus physical microphone/ducking/Booth/recording/dropout qualification and long-session evidence required by the witness.
- M4: real Windows 11 connected-library/session witness from the exact staged executable.
- UI: PR #76 must pass exact-final-head native build/test/package checks and the automated Windows UI pixel witness, then still needs a real Windows 11 visual review at the normal workstation canvas and the 1050×800 compact minimum. Hosted Windows Server screenshots can expose layout regressions but cannot certify Windows 11 HiDPI quality, manual usability or live performance.
- Public Beta publication remains blocked until the applicable qualification evidence, exact candidate CI/package checks, source/notices/checksums and known-issues review are complete. No public release is authorized yet.

## Next largest step

First make the updated UI visual-witness workflow green on the exact PR head and inspect its compact/workstation PNG artifacts for real overlap/clipping regressions. If the pixels are structurally sound, keep PR #76 focused on release-critical visual/resize fixes only and proceed to the remaining Windows 11/manual M1–M4 qualification gates rather than widening feature scope.
