# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `043fcd0e0b7a95c8f8886b6490454f806bf81cf6`; PR #52 (`M3: add microphone ducking, measured limiter and booth routing`) is merged.
- PR #52 exact-head workflow run `35533540860` completed successfully before merge across the required Linux/Windows/package development gate.
- Active development: PR #53 on `feat/m3-eq-qualification`.
- Current PR #53 head at this checkpoint is the branch head containing deterministic production-EQ response fixtures plus `docs/EQ_VALIDATION.md`; exact-head CI is required before any merge.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA). The new EQ evidence does not close M3 or replace listening/hardware qualification.
- GitHub Releases remains empty; no public BrokeDJ Release is qualified by this checkpoint.

## Active M3 slice: deterministic three-band EQ qualification

1. **Exercise the real Engine path**
   - Generated stereo sine fixtures run through the production JUCE-independent `Engine`, not through a duplicate test-only filter.
   - The fixture settles the existing smoothed controls and measures the final rendered master block at 48 kHz.

2. **Verify broad band behavior without inventing a calibrated curve**
   - 80 Hz checks LOW dominance, 1 kHz checks MID dominance and 8 kHz checks HIGH dominance with deliberately broad regression thresholds.
   - Unity LOW/MID/HIGH verifies nominal level reconstruction; full three-band kill verifies at least 60 dB suppression in the fixture.
   - NaN/Inf band controls must fail to finite bounded output.

3. **Document the current limitation honestly**
   - `docs/EQ_VALIDATION.md` records the present complementary first-order split (approximately 200 Hz / 2.4 kHz boundaries) and the current 0.0–2.0 linear band-gain range.
   - This is deterministic software evidence, not a claim of commercial isolator parity, phase-linear behavior or reviewed listening quality.

## Gates still open

- PR #53 must stay unmerged until its exact-final-head Linux/Windows/package run is green; normal BrokeDJ default-branch integration cadence still applies.
- M1 still requires real Windows 11 clean-machine/manual resize/HiDPI/import/device-switching checks and physical four-output master 1/2 versus cue 3/4 verification; six-output Booth also needs physical interface validation before support claims.
- Representative user-owned/licensed music-domain BPM/key/grid evidence remains open.
- Production key-lock listening/latency, MIDI/controller mappings and concrete controller profiles remain unqualified.
- M3 still needs reviewed EQ/listening evidence plus physical microphone/limiter/Booth/recording/long-session evidence. Automated tests do not certify a particular interface or monitoring chain.
- Physical storage/underrun behavior, multi-hour soak and public alpha/beta Release qualification remain open.

## Next largest step

Require a fully green exact-final-head run for PR #53 and repair any regression before integration. After this internally testable EQ gap is closed, keep FINISH FIRST: do not widen M3 with optional features; use the next run for the highest-value remaining current-target qualification or blocker-removal work while the physical Windows/audio-interface gates stay explicit.
