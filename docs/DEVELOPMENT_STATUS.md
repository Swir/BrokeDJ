# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `e21219a6f11e5bc6faf0746edeba52c132bc120f`; PR #51 (`M3: add dropout-aware master set recording`) is merged.
- PR #51 exact-head workflow run `35528118945` completed successfully before merge across the required Linux/Windows development gate.
- Active development: `feat/m3-mic-duck-limiter`; the functional core/app head before this documentation checkpoint is `1e9847963c2ca7eae49b3657a70b4259c5088e6c`.
- The active slice adds opt-in microphone input/ducking and a linked-stereo sample-peak master limiter before the set-recording tap. It still requires exact-final-head Linux/Windows/package CI before merge.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). This slice does not close M3 by itself.
- GitHub Releases remains empty; no public BrokeDJ Release is qualified by this checkpoint.

## Active M3 slice: microphone ducking and measured master protection

1. **Opt-in microphone workflow**
   - BrokeDJ still launches output-only by default. `MIC I/O` lets the user explicitly enable an input channel through JUCE's native device selector.
   - `MIC` fails closed when no active input is available. When enabled, the first active input is mixed into master 1/2 at 0 dB and drives up to 12 dB of music ducking.
   - Cue outputs 3/4 remain outside the microphone/master recording path.

2. **Explicit master sample-peak limiter**
   - A JUCE-independent `MasterPathProcessor` applies linked-stereo, zero-attack sample-peak protection at a -1 dBFS ceiling with a 120 ms release.
   - The limiter is explicitly not a transparent mastering, look-ahead or true-peak limiter. It exposes input/output peak and maximum gain-reduction evidence instead of hiding intervention.
   - Limiter bypass is immediate and removes residual attenuation from a prior reduction event.

3. **Realtime and deterministic evidence**
   - The post-mix processor performs only bounded arithmetic and lock-free atomic control/metric access in the callback; input scratch is allocated during `prepareToPlay()`.
   - `set_recorder` tests now cover below-ceiling transparency, linked-stereo ceiling behavior, ducking response and non-finite input sanitation in addition to the existing WAV/dropout/recovery fixtures.
   - Set recording captures the post-limiter master 1/2 path, while filesystem/WAV encoding remains on the existing background writer.

## Gates still open

- The active M3 slice must remain unmerged until its exact-final-head workflow is fully green on Linux and Windows, including packaged-app smoke.
- M1 still requires real Windows 11 clean-machine/manual resize/HiDPI/import/device-switching checks and physical four-output master 1/2 versus cue 3/4 verification.
- Representative user-owned/licensed music-domain BPM/key/grid evidence remains open.
- Production key-lock listening/latency, MIDI/controller mappings and concrete controller profiles remain unqualified.
- M3 still needs configurable booth/routing and qualified EQ behavior. The new microphone and limiter paths also need real interface/listening/long-session qualification before stronger reliability or sound-quality claims.
- Physical storage/underrun behavior, multi-hour soak and public alpha/beta Release qualification remain open.

## Next largest step

Finish the active microphone/ducking/limiter slice and merge only after exact-final-head Linux/Windows/package CI is green. Then continue the finish-first path toward a usable mixer baseline with the remaining routing/booth and EQ acceptance work, rather than expanding into optional future systems. Keep physical Windows/audio-interface evidence explicit and separate from automated software validation.
