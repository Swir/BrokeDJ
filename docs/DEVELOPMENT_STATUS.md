# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying the already-implemented M1–M4 core workflow before unrelated feature expansion. Optional M5–M8 breadth remains later work unless a concrete Beta blocker requires it.

## Current checkpoint

- Default branch baseline: `main` at `aadfca0630494e6d60c4c7dc9cc91c87e96d73e8`, the squash merge of PR #75 (`Add exact-executable first Beta qualification gate`).
- PR #75 final head `157fb1a0bbc3ff6705cbaf79602a01534043724a` passed its exact-head required workflows before merge: Build and test `35638489968`, Native library scale smoke `35638489923`, Beta qualification tool `35638489893`, M1 hardware witness `35638489888`, M2 key-lock witness `35638489903` and M3 mixer/recording witness `35638490045`.
- GitHub Releases remains empty. No open issue or competing PR was present before the current UI branch was created.
- Active development: draft PR #76, branch `feat/pro-ui-pass`.
- Functional UI implementation checkpoint before this status update: `8ecc9b3c0827f9b297edd38a846cd2b1b6ac09ef`.
- PR #76 adds a BrokeDJ-specific native JUCE LookAndFeel using the canonical dark `#02050A` / `#07111C` and blue→cyan `#0088FF` / `#62E5FF` visual system. It restyles buttons, toggle/focus state, rotary/linear sliders, combo boxes, editors, popups, tooltips and alerts without changing the audio callback or DSP topology.
- The existing recording/microphone/Booth/library controls are visually grouped into one workstation toolbar without changing their routing, ownership or hit targets. The playback-history surface now follows the same visual system.
- The normal workstation default canvas is increased to 1440×960 for more breathing room. The existing 1050×800 minimum and automated resize compatibility sequence remain unchanged, so this is not a claim that the smallest layout is visually qualified.
- This checkpoint documentation commit advances the PR head after the functional implementation commit, so fresh exact-final-head CI remains mandatory before any integration.
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

Run exact-final-head CI for PR #76 and fix every compile, native GUI, package or qualification-tool regression it exposes. Keep the branch unmerged until those checks are green. After automated validation, review the staged Windows UI at normal and minimum sizes on Windows 11 and feed any layout/HiDPI regression back into this same PR before moving on to the remaining M1–M4 manual qualification gates.
