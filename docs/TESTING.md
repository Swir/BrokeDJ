# Validation record

## Verified baseline: 2026-09-18

Previous `main` commit `c16bc227ebb33316b4c551750b7f12bbcd63f88d` completed GitHub Actions workflow run `35316793560` successfully. That run included:

- Linux x64 core configure/build/test with AddressSanitizer and UndefinedBehaviorSanitizer enabled;
- Windows Server 2022 / MSVC x64 configure and Release build of the JUCE application;
- core CTest on Windows;
- native GUI lifecycle smoke mode with a 30-second timeout and no required audio hardware;
- staged development files and corresponding source artifact.

The original core suite reported **438 assertions/checks**. Four hundred are repeated publication operations in the concurrent handoff stress scenario; they are not 400 different feature tests.

## Audio quality hardening branch

The development branch adds a second deterministic CTest executable, `brokedj_quality_tests`. Offline compilation of the modified JUCE-independent core and this test executable succeeded, and the quality suite passed **10 checks** covering:

- playback reaches a stable level before transition tests;
- pause starts near the prior processed sample and decays instead of hard-cutting;
- seek across a polarity step avoids an immediate polarity discontinuity and reaches destination audio after the transition;
- EQ/echo/drive automation remains finite under simultaneous control changes.

These tests support the implementation of short transport transitions and control smoothing. They do **not** prove inaudibility on every file, buffer size, device or loudspeaker chain.

## Existing core coverage

Empty silence; invalid clip/deck/device-rate rejection; stereo playback; playhead/rate/seek behavior; crossfader endpoints; cue isolation on two/four outputs; EOF; whole-track looping; clipping/finite values; three-band kill; an echo impulse; stopped replacement; retirement backpressure; concurrent publication and shutdown.

## What automated checks do not certify

Automated CI does not by itself certify Windows 11 clean-machine usability, physical audio hardware, device switching, real four-output cue isolation, controller support, latency, listening quality, zero audible clicks, ASIO support, or multi-hour live reliability. The Catmull-Rom rate converter is still pitch-changing resampling, not key lock/time stretch. No GUI screenshot or hardware result should be fabricated as evidence.

## Native acceptance checklist

- [x] Windows x64 build and native no-audio smoke mode pass in CI on the verified baseline.
- [ ] Exact-head CI passes for the current audio-quality branch.
- [ ] Clean Windows 11 machine launches and logs startup correctly.
- [ ] Mono/stereo WAV, FLAC, OGG, AIFF, CBR/VBR MP3 fixtures decode as expected.
- [ ] Invalid, truncated, Unicode-path and oversized files fail clearly without losing working audio.
- [ ] Minimum and large window sizes keep every control reachable.
- [ ] Two-output and four-output physical devices have correct master/cue isolation.
- [ ] Device change/disconnect and closing during decode recover safely.
- [ ] Sample-rate/buffer changes are exercised without invalid output or transport corruption.
- [ ] Long-running simultaneous playback and repeated loading pass an agreed soak test.
- [ ] Transport/seek transitions receive deterministic render metrics plus reviewed listening checks on representative fixtures.

Use original/generated or appropriately licensed audio fixtures only. Report the commit, OS, device/driver, sample rate, buffer size, reproduction steps and a reviewed/redacted log. Do not mark a release gate complete based only on a scheduled run, a screenshot or a compile result.
