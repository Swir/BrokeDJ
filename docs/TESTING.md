# Validation record

## Verified automation: 2026-09-18

Audio-quality hardening PR #1 was verified at exact head `a3f29da6e753e9f27b00356ccebe5644101d1a27` by GitHub Actions run `35328799488`, then merged to `main` as `977ad6a16de0837666208d6b8b1a9459191a7f17`.

That successful run included:

- Linux x64 core configure/build/test with AddressSanitizer and UndefinedBehaviorSanitizer enabled;
- generated SVG progress synchronization plus legacy character-meter rejection;
- Windows Server 2022 / MSVC x64 configure and Release build of the JUCE application;
- core and audio-quality CTest on Windows;
- native GUI lifecycle smoke mode with a 30-second timeout and no required audio hardware;
- staged development files and corresponding source artifact upload.

The original merged core suite reported **438 assertions/checks**. Four hundred are repeated publication operations in the concurrent handoff stress scenario; they are not 400 different feature tests.

## Current transport-continuity package

The current development branch extends continuity hardening without changing the roadmap counter. Before publication, the JUCE-independent C++20 core was compiled locally in optimized and AddressSanitizer/UndefinedBehaviorSanitizer configurations. Both suites passed: **439 core checks and 18 quality checks**.

New deterministic coverage verifies:

- a playback-rate change slews from the old speed rather than jumping instantly, then converges to the requested target;
- whole-track loop wraparound retains the previous processed polarity at the start of the short transition and reaches the wrapped audio afterward;
- disabling headphone cue fades from the prior cue level instead of hard-cutting;
- existing pause, seek and simultaneous EQ/echo/drive automation remain finite and continuous under the established checks.

Exact-head GitHub Actions remains required before merge. Local sanitizer success is supporting evidence, not a substitute for the repository's Windows build/smoke gate or physical audio-interface testing.

## Audio quality hardening coverage

The repository has a second deterministic CTest executable, `brokedj_quality_tests`. Its checks cover stable playback, pause/seek transition behavior, loop-wrap continuity, cue switching continuity and control automation. These tests support short transport/cue transitions and control smoothing. They do **not** prove inaudibility on every file, buffer size, device or loudspeaker chain.

## Existing core coverage

Empty silence; invalid clip/deck/device-rate rejection; stereo playback; playhead/rate/seek behavior; crossfader endpoints; cue isolation on two/four outputs; EOF; whole-track looping; clipping/finite values; three-band kill; an echo impulse; stopped replacement; retirement backpressure; concurrent publication and shutdown.

## What automated checks do not certify

Automated CI does not by itself certify Windows 11 clean-machine usability, physical audio hardware, device switching, real four-output cue isolation, controller support, latency, listening quality, zero audible clicks, ASIO support, or multi-hour live reliability. The Catmull-Rom rate converter is still pitch-changing resampling, not key lock/time stretch. No GUI screenshot or hardware result should be fabricated as evidence.

## Native acceptance checklist

- [x] Windows x64 build and native no-audio smoke mode pass in CI on the merged audio-quality package.
- [x] Exact-head PR CI passed before merge for the audio-quality package.
- [ ] Current transport-continuity branch passes exact-head PR CI.
- [ ] Clean Windows 11 machine launches and logs startup correctly.
- [ ] Mono/stereo WAV, FLAC, OGG, AIFF, CBR/VBR MP3 fixtures decode as expected.
- [ ] Invalid, truncated, Unicode-path and oversized files fail clearly without losing working audio.
- [ ] Minimum and large window sizes keep every control reachable.
- [ ] Two-output and four-output physical devices have correct master/cue isolation.
- [ ] Device change/disconnect and closing during decode recover safely.
- [ ] Sample-rate/buffer changes are exercised without invalid output or transport corruption.
- [ ] Long-running simultaneous playback and repeated loading pass an agreed soak test.
- [ ] Transport/seek/loop/cue transitions receive deterministic render metrics plus reviewed listening checks on representative fixtures.

Use original/generated or appropriately licensed audio fixtures only. Report the commit, OS, device/driver, sample rate, buffer size, reproduction steps and a reviewed/redacted log. Do not mark a release gate complete based only on a scheduled run, a screenshot or a compile result.
