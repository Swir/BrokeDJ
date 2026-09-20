# BrokeDJ three-band EQ validation

This document records deterministic software evidence for the current BrokeDJ deck EQ. It is not a listening certification, a hardware qualification or a claim that the current EQ matches a commercial DJ isolator.

## Current implementation

The deck EQ is a complementary three-band split inside `src/core/Engine.cpp`:

- low band: first-order low-pass state around 200 Hz;
- mid band: the difference between the approximately 2.4 kHz low-pass state and the low-band state;
- high band: the input minus the approximately 2.4 kHz low-pass state;
- all three gains at unity reconstruct the input algebraically from the same states;
- all three gains at zero produce a full three-band kill;
- controls are smoothed in the audio path and non-finite controls fall back to finite bounded values.

The control range remains the current 0.0–2.0 linear band gain. This is intentionally documented rather than described as a calibrated ±dB DJ EQ curve.

## Deterministic acceptance fixture

`tests/CoreTests.cpp` renders generated stereo sine clips through the real `Engine` path at 48 kHz after control settling. The test checks broad band selectivity instead of fragile exact floating-point transfer values:

- 80 Hz: LOW must dominate MID by more than 2× and HIGH by more than 8×;
- 1 kHz: MID must dominate LOW by more than 3× and HIGH by more than 2×;
- 8 kHz: HIGH must dominate LOW by more than 8× and MID by more than 2.5×;
- unity LOW/MID/HIGH must retain the nominal 1 kHz fixture level;
- full LOW/MID/HIGH kill must suppress the 1 kHz fixture by at least 60 dB relative to unity;
- NaN/Inf EQ controls must not poison the rendered block with non-finite samples.

These thresholds are intentionally loose enough to verify the documented topology and fail on regressions without pretending the current first-order split is a final mastered EQ design.

## Still outside proof

- reviewed listening across representative music and monitoring systems;
- physical audio-interface behavior and gain staging;
- perceptual comparison with commercial DJ EQ/isolator curves;
- calibrated analog-model emulation or phase-linear behavior;
- long-session/controller automation qualification.

Those remain separate manual/hardware gates. A future EQ redesign must update this document and its deterministic response fixtures rather than silently changing the curve.
