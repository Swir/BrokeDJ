# Contributing

BrokeDJ targets a full professional workflow, but every shipped claim must match implemented and tested behavior. Keep core DSP separate from JUCE adapters. Never perform file I/O, blocking waits or heap allocation/deallocation in the audio callback. Document any bounded but potentially expensive callback work and measure it.

Run core tests and update the validation record. A bug fix needs a regression test where practical. Preserve Polish/English behavior and existing controls. Import third-party code only with provenance and a compatible license; do not add music, proprietary plugins, credentials or sample packs without distribution rights.

Use small reviewable commits. Update README, ROADMAP, third-party notices and the generated SVG when their source facts change. Search Keywords must remain accurate and natural. Do not substitute a simulated UI for a verified native screenshot, advertise planned features as implemented, or mark a development artifact as production-ready.
