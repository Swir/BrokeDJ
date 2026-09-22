# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying the already-implemented M1–M4 core workflow before unrelated feature expansion. Optional M5–M8 breadth remains later work unless a concrete Beta blocker requires it.

## Current checkpoint

- Default branch baseline: `main` at `a1d9e988784066c9ed1d3b498c9a543f614cde87`, the squash merge of PR #77 (`Improve Beta UI control hierarchy and legibility`). PR #77 was integrated after its exact-head Windows Build and test, native library-scale smoke and UI visual witness were green and the compact/workstation PNGs were reviewed. The merge changes presentation only; it does not change Engine/DSP/routing/device/decoder ownership.
- Active development: draft PR #78 on `feat/ui-visual-quality-gate`. Product implementation head `0193caaff12f499b707c82e97380c4a1606a7319` combines the deterministic Windows pixel sanity gate with the issue #79 workstation geometry regression fix. It caches the full 22-button deck inventory, lays out all eight Hot Cues, binds Reset grid / Tempo map to their actual controls, and makes the no-audio native resize smoke reject out-of-bounds or overlapping deck controls in workstation mode.
- Exact implementation-head CI: Build and test run `35742821840`, Native library scale smoke run `35742821785`, and UI visual witness run `35742821807` were still in progress when this checkpoint was written. The PR must not be merged or called qualified until those exact-head runs are green and the newly generated compact/workstation PNGs plus pixel-quality report are reviewed.
- The UI witness gate measures broad luminance, luminance variation, bright-detail fraction, BrokeDJ blue/cyan accent presence, quantized colour diversity and painted-content fraction. The new native geometry contract complements that pixel gate by checking component ownership, containment and pairwise button/combobox overlap. Neither mechanism opens audio or certifies aesthetics, accessibility, HiDPI usability, controllers or live performance.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. UI integration, regression repair and witness hardening do not advance the milestone counter.
- GitHub Releases is still empty; no public Beta/Release is authorized.

## Integrated foundations on main

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, a silent device-capability probe and a packaged privacy-safe hardware witness recorder. Real clean-machine playback, device switching and physical master 1/2 versus CUE 3/4 isolation remain manual.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH, the opt-in Signalsmith-backed key-lock research lifecycle and a packaged privacy-safe listening witness recorder. Representative music-domain and real listening/latency/hardware evidence remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, opt-in microphone ducking, limiter measurements, deterministic EQ qualification and a packaged mixer/recording witness. Physical/listening gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe backup/restore and native 5k-track/12k-history staged-EXE recovery.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not make a public Beta Release by itself.
- The main workstation UI now has four deck surfaces around a dedicated four-channel center mixer, responsive compact fallback, stronger knob/fader hierarchy and restrained semantic state accents. Hosted CI pixels are regression evidence only; real Windows 11 manual visual review remains open.

## Gates still open before first Beta qualification

- M1: real Windows 11 clean launch/resize/import/playback/device-switch/four-output-cue witness on actual hardware.
- M2: representative music-domain BPM/key evidence, real key-lock listening evidence, device CPU/callback-deadline/underrun and latency qualification, plus controller/wider scratch qualification required by the current milestone wording.
- M3: real Windows 11 reviewed EQ/mixer listening plus physical microphone/ducking/Booth/recording/dropout qualification and long-session evidence required by the witness.
- M4: real Windows 11 connected-library/session witness from the exact staged executable.
- UI: PR #78 must pass the product implementation head Windows build/tests plus native geometry and pixel-quality witness gates; both PNGs and the JSON metrics must be reviewed. Any later checkpoint-only commit must still be treated as a distinct PR head for the final merge gate.
- Public Beta publication remains blocked until the applicable qualification evidence, exact candidate CI/package checks, source/notices/checksums and known-issues review are complete. No public release is authorized yet.

## Next largest step

Finish PR #78 validation on `0193caaff12f499b707c82e97380c4a1606a7319`, inspect its compact/workstation PNGs and quality report, and fix any real geometry or capture failure rather than weakening the gates. Once the final PR head has the required green evidence, integrate this Beta qualification slice; then return to the frozen M1–M4 real Windows hardware/listening/library-session witnesses instead of widening product scope.
