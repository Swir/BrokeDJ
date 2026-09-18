# Time-stretch / key-lock prototype

This document records an **opt-in M2 engineering prototype**, not a user-facing BrokeDJ feature or a release-readiness claim. Ordinary BrokeDJ playback continues to use the existing hybrid Catmull-Rom/windowed-sinc rate converter and does not depend on this prototype.

## Dependency and license review

The prototype uses official upstream sources only:

- Signalsmith Stretch, pinned to commit `57b93f4e9206a089a45387eaa39bdc9f310d3308`, MIT license.
- Signalsmith Linear, pinned to commit `5668673560146a9cfe38c25315071e3fd68c8317` (upstream tag `0.3.1`), MIT license.

Both dependencies are fetched only when `BROKEDJ_BUILD_TIMESTRETCH_PROTOTYPE=ON`. The option defaults to `OFF`, so the normal core and application build do not acquire or compile this research path.

## Architecture boundary

`src/core/TimeStretch.*` exposes a small JUCE-independent stereo adapter. `prepare()` creates/configures the processor and establishes explicit maximum input/output block sizes outside the audio callback. Processing rejects null, empty or oversized blocks rather than performing unbounded work. Pitch is currently limited to ±24 semitones and seek-preroll playback-rate hints to 0.25–4.0x.

The adapter is intentionally **not connected to `Engine` yet**. Integration requires a separate reviewed design for source read-ahead, input/output clocking, transport reset/seek, latency compensation, smooth enable/bypass transitions and failure fallback. The existing pitch-changing rate converter remains available even if the prototype is disabled or later rejected.

## Automated evidence

With the prototype enabled, CTest adds:

- `time_stretch`: configuration/bounds, reported input/output latency, neutral 1.25x time-ratio pitch preservation, explicit +12-semitone pitch shift, reset and seek-preroll behavior, finite output.
- `time_stretch_realtime`: warmed 1.25x processing with bounded pitch automation while heap allocation/deallocation tracking is active. The measured processing window must report zero heap allocations and deallocations.

Timing printed by the realtime test is diagnostic only. Shared CI runners do not establish supported buffer sizes, hardware latency, CPU headroom or dropout-free live performance.

## Integration gate

Do not replace or bypass the production fallback until all of the following have evidence:

1. Exact-head Linux sanitizer and Windows x64 CI pass with the prototype enabled.
2. Ratio, pitch, latency, reset/seek and zero-heap processing tests remain deterministic.
3. A bounded deck-side buffering/clocking design handles time-ratio changes without disk I/O, decoding, blocking synchronization or allocation in the audio callback.
4. Enable/disable, seek, loop and load transitions are de-clicked and covered by render tests.
5. Representative music-domain listening and CPU/underrun measurements are performed on explicit Windows 11 hardware configurations.
6. Third-party license notices/source obligations remain complete for any distributed build that actually includes the dependency.

Until those gates pass, this is research infrastructure for M2, not production key lock.
