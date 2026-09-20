# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `e21219a6f11e5bc6faf0746edeba52c132bc120f`; PR #51 (`M3: add dropout-aware master set recording`) is merged.
- Active development remains PR #52 on `feat/m3-mic-duck-limiter`; do not create a competing M3 branch while this package is open.
- Parent checkpoint `0ee858bf8acdf35a43f1d09590283e135ca311ce` passed exact-head workflow run `35532349004` across the required Linux/Windows/package development gate.
- The current branch package extends that green microphone/ducking/limiter slice with fail-closed dedicated Booth routing on logical outputs 5/6, independent attenuation and deterministic routing tests. This newer checkpoint requires its own exact-head CI before merge.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). M3 remains open.
- GitHub Releases remains empty; no public BrokeDJ Release is qualified by this checkpoint.

## Active M3 slice: usable master, microphone, booth and recording path

1. **Opt-in microphone workflow**
   - BrokeDJ still launches output-only by default. `MIC I/O` lets the user explicitly enable an input channel through JUCE's native device selector.
   - `MIC` fails closed when no active input is available. When enabled, the first active input is mixed into master 1/2 at 0 dB and drives up to 12 dB of music ducking.
   - Device re-prepare without an input disables the microphone path instead of leaving a stale armed state.

2. **Measured master protection and recording**
   - A JUCE-independent `MasterPathProcessor` applies linked-stereo, zero-attack sample-peak protection at a -1 dBFS ceiling with a 120 ms release.
   - The limiter is explicitly not a transparent mastering, look-ahead or true-peak limiter. It exposes input/output peak and maximum gain-reduction evidence instead of hiding intervention.
   - Set recording captures only the post-limiter master 1/2 path; filesystem/WAV encoding remains on the existing background writer and private cue is never folded into the recording.

3. **Dedicated Booth route without cue leakage**
   - The device selector now permits up to six output channels. Master remains logical outputs 1/2, private cue remains 3/4 and optional Booth uses 5/6 only when six outputs are active.
   - Booth copies the protected master after the limiter and has an independent smoothed attenuation-only level from -60 to 0 dB; it cannot boost above the protected master.
   - Channels above the Engine-owned master/cue buses are explicitly cleared before Booth generation so stale input/device samples cannot leak to the dedicated route.
   - If six outputs disappear on device re-prepare, Booth disables fail-closed. Set recording still captures master 1/2 only, never cue or Booth.

4. **Deterministic and realtime evidence**
   - The post-mix processor performs bounded arithmetic and lock-free atomic control/metric access in the callback; microphone scratch is allocated during `prepareToPlay()`.
   - `set_recorder` tests cover below-ceiling transparency, sustained linked-stereo ceiling behavior, ducking, non-finite sanitation, protected-master Booth copying, independent Booth level and explicit clearing when Booth is disabled.
   - The core Booth processor was also compiled locally as a standalone C++20 header fixture with strict GCC warnings before publication; native JUCE/MSVC behavior remains gated by exact-head CI.

## Gates still open

- The newest PR #52 checkpoint must remain unmerged until its exact-head workflow is fully green on Linux and Windows, including packaged-app smoke.
- M1 still requires real Windows 11 clean-machine/manual resize/HiDPI/import/device-switching checks and physical four-output master 1/2 versus cue 3/4 verification; six-output Booth also needs physical interface validation before support claims.
- Representative user-owned/licensed music-domain BPM/key/grid evidence remains open.
- Production key-lock listening/latency, MIDI/controller mappings and concrete controller profiles remain unqualified.
- M3 still needs qualified EQ behavior and physical microphone/limiter/Booth/listening/long-session evidence. Automated routing tests do not certify a particular interface.
- Physical storage/underrun behavior, multi-hour soak and public alpha/beta Release qualification remain open.

## Next largest step

Require a fully green exact-head Linux/Windows/package run for the current PR #52 checkpoint and repair any regression before merge. Then continue the FINISH FIRST path with the remaining internally closable M3 EQ acceptance/routing evidence instead of expanding into optional future systems. Keep physical Windows/audio-interface evidence explicit and separate from automated software validation.
