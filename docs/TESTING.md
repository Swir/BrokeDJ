# Validation record

## Local checks: 2026-09-18

Environment: Linux x64, GCC 14.2.0, CMake 3.31.6, Ninja 1.12.1. Core build configured with `BROKEDJ_BUILD_APP=OFF`, Debug, AddressSanitizer and UndefinedBehaviorSanitizer.

Result: **one CTest suite passed, 438 assertions/checks completed**. Four hundred checks are repeated publication operations in the concurrent handoff stress scenario; they are not 400 different feature tests.

Covered: empty silence; invalid clip/deck/device-rate rejection; stereo playback; playhead/rate/seek behavior; crossfader endpoints; cue isolation on two/four outputs; EOF; whole-track looping; clipping/finite values; three-band kill; an echo impulse; stopped replacement; retirement backpressure; concurrent publication and shutdown.

## Not verified by those checks

Native JUCE compilation and startup are separate CI jobs. Windows hardware playback, device switching, waveform interaction, controller support, latency, sound quality, de-clicking, output routing on a real interface and multi-hour stability are not certified by a pure-core test pass. No GUI screenshot is fabricated as evidence. Sanitizers do not prove hard real-time safety or absence of data races.

## Native acceptance checklist

- [ ] Windows x64 build and native smoke mode pass in CI.
- [ ] Clean Windows 11 machine launches and logs startup correctly.
- [ ] Mono/stereo WAV, FLAC, OGG, AIFF, CBR/VBR MP3 fixtures decode as expected.
- [ ] Invalid, truncated, Unicode-path and oversized files fail clearly without losing working audio.
- [ ] Minimum and large window sizes keep every control reachable.
- [ ] Two-output and four-output devices have correct master/cue isolation.
- [ ] Device change/disconnect and closing during decode recover safely.
- [ ] Long-running simultaneous playback and repeated loading pass an agreed soak test.

Use original/generated or appropriately licensed audio fixtures only. Report the commit, OS, device/driver, sample rate, buffer size, reproduction steps and a reviewed/redacted log. Do not mark a release gate complete based only on a scheduled run or a screenshot.
