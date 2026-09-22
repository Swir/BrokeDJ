# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying the already-implemented M1–M4 core workflow before unrelated feature expansion. Optional M5–M8 breadth remains later work unless a concrete Beta blocker requires it.

## Current checkpoint

- Default branch baseline: `main` at `aadfca0630494e6d60c4c7dc9cc91c87e96d73e8`, the squash merge of PR #75 (`Add exact-executable first Beta qualification gate`).
- PR #75 final head `157fb1a0bbc3ff6705cbaf79602a01534043724a` passed its exact-head required workflows before merge: Build and test `35638489968`, Native library scale smoke `35638489923`, Beta qualification tool `35638489893`, M1 hardware witness `35638489888`, M2 key-lock witness `35638489903` and M3 mixer/recording witness `35638490045`.
- GitHub Releases remains empty. No open issue or competing PR was present before the current UI branch was created.
- Active development: draft PR #76, branch `feat/pro-ui-pass`.
- Latest functional UI checkpoint: `0e3bc6ca427782ed805a301ad7e9aedd6351c005` (`Rebuild the four-deck workstation around a central mixer`).
- At workstation sizes (base component at least 1240×850), Decks A/C now occupy the left performance column and B/D the right column around a dedicated central mixer. Existing per-deck Trim, LOW/MID/HIGH and channel-fader controls are reparented into four channel strips while preserving their existing callbacks and Engine ownership; Rate, Echo and Drive remain deck-local.
- The central mixer also owns the existing master meter, Master, Headphones and Crossfader controls in a bottom master shelf. The deck panels reclaim the former bottom mixer-strip space for waveform/performance controls and use a deterministic compact reflow rather than adding placeholder controls.
- Below the workstation breakpoint, controls are returned to their original deck parents/styles and the established compact layout remains available. The existing no-audio GUI smoke sequence therefore crosses compact and workstation paths (1050×800, 1280×860, 1600×900, 1050×800) without requiring audio hardware.
- The native JUCE LookAndFeel continues to use the canonical dark `#02050A` / `#07111C` and blue→cyan `#0088FF` / `#62E5FF` visual system with workstation-style buttons, rotary controls, faders and combo boxes. Recording, microphone and Booth retain distinct operational states; no audio/DSP topology changed in this UI pass.
- Exact-head validation for functional checkpoint `0e3bc6ca...`: Build and test run `35720478440` has a green Core/sanitizer build and is still running the Windows x64 job at the time of this checkpoint; Native library scale smoke `35720478488` is still running. M1 hardware witness `35720478437`, M2 key-lock witness `35720478419`, M3 mixer/recording witness `35720478511` and Beta qualification tool `35720478453` completed successfully as tooling/workflow checks. The PR must not be merged until the final branch head has fresh required green checks.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. UI restructuring, polish and Beta tooling do not advance the milestone counter by themselves.

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
- UI: PR #76 must pass exact-final-head native build/test/package checks and still needs a real Windows visual review at the normal 1440×960 workstation canvas and the 1050×800 compact minimum. Automated resize smoke proves geometry/lifecycle execution only, not professional appearance, HiDPI usability or visual quality.
- Public Beta publication remains blocked until the applicable qualification evidence, exact candidate CI/package checks, source/notices/checksums and known-issues review are complete. No public release is authorized yet.

## Next largest step

First resolve any exact-head compile, GUI lifecycle, package or scale-smoke regression caused by the central-mixer reflow. Once the final PR head is automated-green, inspect the staged Windows executable at 1440×960 and 1050×800 on Windows 11 and feed any real overlap/HiDPI/visual regressions back into this same PR. After the UI pass is visually accepted, continue the existing M1–M4 physical/listening qualification gates rather than widening feature scope.