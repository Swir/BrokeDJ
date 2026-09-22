# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying the already-implemented M1–M4 core workflow before unrelated feature expansion. Optional M5–M8 breadth remains later work unless a concrete Beta blocker requires it.

## Current checkpoint

- Default branch baseline: `main` at `aadfca0630494e6d60c4c7dc9cc91c87e96d73e8`, the squash merge of PR #75 (`Add exact-executable first Beta qualification gate`).
- PR #75 final head `157fb1a0bbc3ff6705cbaf79602a01534043724a` passed its exact-head required workflows before merge: Build and test `35638489968`, Native library scale smoke `35638489923`, Beta qualification tool `35638489893`, M1 hardware witness `35638489888`, M2 key-lock witness `35638489903` and M3 mixer/recording witness `35638490045`.
- GitHub Releases remains empty. No open issue or competing PR was present before the current UI branch was created.
- Active development: draft PR #76, branch `feat/pro-ui-pass`.
- Latest functional UI checkpoint: `a6f4d27903a1bf14bb73260c6d8cbdb823ff01d0` (`Polish DJ controls and workflow strip`).
- The native JUCE LookAndFeel now gives buttons, faders, rotary controls and combo boxes a deeper workstation-style material hierarchy: recessed tracks/wells, restrained gradients, hardware-like knob ticks/fader caps, clearer hover/focus state and thin status illumination while preserving per-control state colours.
- Operational state is easier to scan without touching audio behavior: recording uses a dedicated red active state, microphone uses an amber active state, Booth level gets a distinct monitor signal colour, and the existing record/dynamics-monitor/library controls sit inside a segmented top workstation shell. No routing, callback, decoder, device or DSP topology changed in this pass.
- The earlier PR work still provides the canonical dark `#02050A` / `#07111C` and blue→cyan `#0088FF` / `#62E5FF` visual system, consistent history/library surfaces and a 1440×960 normal canvas while retaining the existing 1050×800 automated resize compatibility gate.
- Functional head `a6f4d279...` started exact-head workflows `35717414096` (Build and test) and `35717414051` (Native library scale smoke). They were still running when this checkpoint was written; this documentation commit advances the PR head again, so the final head must receive fresh exact-head results before integration.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. UI polish and Beta tooling do not advance the milestone counter by themselves.

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
- UI: PR #76 must pass exact-final-head native build/test/package checks and still needs a real Windows visual review; automated resize smoke proves geometry/lifecycle only, not professional appearance or HiDPI usability.
- Public Beta publication remains blocked until the applicable qualification evidence, exact candidate CI/package checks, source/notices/checksums and known-issues review are complete. No public release is authorized yet.

## Next largest step

Fix any exact-head compile, GUI lifecycle, package or scale-smoke regression from PR #76 before integration. Once the final head is automated-green, review the staged Windows UI at 1440×960 and the 1050×800 minimum on Windows 11; feed layout/HiDPI regressions back into this same PR, then continue the existing M1–M4 physical/listening qualification gates instead of widening feature scope.
