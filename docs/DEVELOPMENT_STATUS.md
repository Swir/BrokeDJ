# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying the already-implemented M1–M4 core workflow before unrelated feature expansion. Optional M5–M8 breadth remains later work unless a concrete Beta blocker requires it.

## Current checkpoint

- Default branch baseline: `main` at `c7412076ba9d685b449c3a241409aaa605e0f1c1`, the squash merge of PR #83 (`Exercise native session restore through the audio ownership boundary`).
- PR #83 final head `9234a7aacdeffe3ed130ed4adc4e96f73fa076b0` passed exact-head Build and test run #424 and Native library scale smoke run #94 before integration. Build #424 included Linux ASan/UBSan/core checks, Windows x64 build/full CTest, native no-audio GUI resize/geometry smoke, silent device enumeration, staged package verification and staged executable smoke. Run #94 passed the native and staged 5k-track/12k-history library lifecycle qualification.
- The integrated native session regression saves/checksum-loads a four-deck snapshot, explicitly verifies `.bak` recovery after primary corruption, restores three sources through the real async decoder and `Engine` adoption boundary, clears one saved empty deck through the audio-owned eject path, restores controls/paused seek positions only after adoption and proves saved `wasPlaying=true` never auto-resumes.
- That regression is offline and synthetic: it opens no physical audio device and emits no audible output. It strengthens automated M4 prerequisite evidence but does not replace the connected Windows 11 library/session witness.
- There is no active BrokeDJ feature PR at this checkpoint. The current internally implemented Beta slice is held against its manual/hardware qualification gates rather than widened into M5+ feature work.
- Roadmap source of truth remains `docs/progress.json`: **1/10 equal-weight milestones complete (10.0%, PRE-ALPHA)**. M1–M4 remain open until their documented acceptance evidence exists.
- GitHub Releases is still empty; no public Beta/Release is authorized.

## Integrated foundations on main

- M1 has native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, a silent device-capability probe, a packaged privacy-safe hardware witness recorder, a deterministic portable Beta Preview candidate and native four-deck async import/replacement isolation. Real clean-machine import/playback, device switching and physical master 1/2 versus CUE 3/4 isolation remain manual.
- M2 has reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH, the opt-in Signalsmith-backed key-lock research lifecycle and a packaged privacy-safe listening witness recorder. Representative music-domain and real listening/latency/hardware evidence remain open.
- M3 has channel trim, meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, opt-in microphone ducking, limiter measurements, deterministic EQ qualification and a packaged mixer/recording witness. Physical/listening gates remain open.
- M4 library/session behavior is internally implemented and automated at scale: SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery and the native async session/adoption round trip integrated in PR #83. Its human witness rejects CI evidence generation, requires Windows 11 x64/x64 PowerShell and preflights the exact staged executable with the no-audio resize/geometry smoke before manual checks.
- The Beta qualification orchestrator composes privacy-safe M1–M4 witness files only when they match one exact staged executable/source identity. It cannot replace human hardware/listening workflows and does not make a public Beta Release by itself.
- The main workstation UI has four deck surfaces around a dedicated four-channel center mixer, responsive compact fallback, stronger knob/fader hierarchy, restrained semantic state accents and deterministic Windows geometry/pixel witnesses. Hosted CI pixels are regression evidence only; real Windows 11 manual visual review remains open.

## Gates still open before first Beta qualification

- M1: real Windows 11 clean launch/resize/import/playback/device-switch/four-output-cue witness on actual hardware.
- M2: representative music-domain BPM/key evidence, real key-lock listening evidence, device CPU/callback-deadline/underrun and latency qualification, plus controller/wider scratch qualification required by the current milestone wording.
- M3: real Windows 11 reviewed EQ/mixer listening plus physical microphone/ducking/Booth/recording/dropout qualification and long-session evidence required by the witness.
- M4: privacy-safe connected Windows 11 library/session witness from the exact staged executable.
- Public Beta publication remains blocked until the applicable qualification evidence, exact candidate CI/package checks, source/notices/checksums and known-issues review are complete. No public release is authorized yet.

## Next largest step

Run and review the documented privacy-safe M4 connected-library/session witness on Windows 11 x64 against one exact staged executable. Fix any real regression it exposes before changing M4 status. Then complete the frozen M1–M3 hardware/listening evidence. Do not widen the current Beta target into M5+ feature work merely to keep the automation busy; the remaining blockers are now explicit external/manual qualification gates.
