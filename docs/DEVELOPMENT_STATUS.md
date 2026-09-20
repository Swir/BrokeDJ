# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `d9fe4cc78166fcb6517c6a2c14566204ce4f42cd`; PR #50 (`M3: add input trim and richer mixer metering`) is merged.
- PR #50 exact-head workflow run `35526050200` completed successfully before merge: Linux sanitizer/full CTest plus Windows x64 build/full CTest/audio diagnostics, native no-audio GUI lifecycle+resize smoke, silent device probe, staging and package verification passed.
- Active development: PR #51 (`M3: add dropout-aware master set recording`) from `feat/m3-dropout-aware-recording`.
- Functional recording head `0f8df785c0c1cec20ac77ffd90bf278272fe20c3` adds the native recording workflow and deterministic recorder tests. Its workflow run `35527046977` is queued at this checkpoint, so PR #51 remains unmerged.
- This documentation commit creates a newer final branch head and therefore also requires its own exact-head Linux/Windows/package gate before PR #51 may merge.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). Recording does not close M3 by itself.
- GitHub Releases remains empty; no public BrokeDJ Release is qualified by this checkpoint.

## Active M3 slice: dropout-aware set recording

1. **Record the actual master path without private cue**
   - The native app can record already-rendered master outputs 1/2 to a stereo 24-bit WAV.
   - Cue outputs 3/4 are not folded into the recording path.
   - PL/EN `REC SET` / `STOP REC` controls use a native save chooser and do not overwrite an existing user file.

2. **Bounded realtime handoff and explicit dropout evidence**
   - The audio callback performs only bounded copies into a fixed-capacity SPSC FIFO plus lock-free counters; WAV encoding and filesystem I/O stay on a dedicated writer thread.
   - FIFO overflow never waits for disk I/O: unavailable frames are dropped from the recording and counted as `droppedFrames` plus `dropoutEvents` so a damaged capture cannot be silently reported as perfect.
   - The recording writer uses a `.part` recovery file and moves it to the requested WAV only after a clean flush/finalization. Writer/rename failure retains recovery evidence instead of replacing the requested destination.

3. **Deterministic integrity coverage**
   - `set_recorder` CTest covers clean stereo WAV finalization, exact sample-rate/frame-count reopening, finite decoded samples, invalid-start fail-closed behavior and deterministic FIFO-overflow accounting.
   - The overflow fixture submits more frames in one bounded publish than the complete test FIFO can hold, so the drop counter is exercised without relying on thread timing or artificial sleeps.
   - This is software-path evidence only; it does not prove real storage throughput, physical audio-interface reliability or multi-hour dropout-free capture.

## Gates still open

- PR #51 must remain unmerged until the newest documentation-inclusive head has a fully green exact-head workflow.
- M1 still requires real Windows 11 clean-machine/manual resize/HiDPI/import/device-switching checks and physical four-output master 1/2 versus cue 3/4 verification.
- Representative user-owned/licensed music-domain BPM/key/grid evidence remains open.
- Production key-lock listening/latency, MIDI/controller mappings and concrete controller profiles remain unqualified.
- M3 still lacks configurable routing/booth, qualified EQ curves and limiter behavior, and microphone/ducking. Set recording also still needs physical-storage and long-session qualification before stronger reliability claims.
- Physical storage/underrun behavior, multi-hour soak and public alpha/beta Release qualification remain open.

## Next largest step

Finish PR #51 first and merge only after its exact-final-head Linux/Windows/package gate is green. Then continue the finish-first product path with the remaining core M3 workflow: explicit master/booth/input routing plus microphone/ducking, keeping device-specific behavior fail-closed when suitable hardware is unavailable. Limiter/EQ refinement and optional polish remain secondary to completing the usable mixer workflow.