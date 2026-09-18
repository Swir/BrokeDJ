# Time-stretch / key-lock prototype

This document records an **opt-in M2 engineering prototype**, not a user-facing BrokeDJ feature or a release-readiness claim. Ordinary BrokeDJ playback continues to use the existing hybrid Catmull-Rom/windowed-sinc rate converter and does not depend on this prototype.

## Dependency and license review

The prototype uses official upstream sources only:

- Signalsmith Stretch, pinned to commit `57b93f4e9206a089a45387eaa39bdc9f310d3308`, MIT license.
- Signalsmith Linear, pinned to commit `5668673560146a9cfe38c25315071e3fd68c8317` (upstream tag `0.3.1`), MIT license.

Both dependencies are fetched only when `BROKEDJ_BUILD_TIMESTRETCH_PROTOTYPE=ON`. The option defaults to `OFF`, so the normal core and application build do not acquire or compile this research path.

## Architecture boundary

The opt-in path now has four JUCE-independent layers:

1. `src/core/TimeStretch.*` wraps the pinned Signalsmith processor. `prepare()` creates/configures the processor and establishes explicit maximum input/output block sizes outside the audio callback. Processing rejects null, empty or oversized blocks rather than performing unbounded work. Pitch is currently limited to ±24 semitones and seek-preroll playback-rate hints to 0.25–4.0x.
2. `src/core/TimeStretchDeck.*` owns the bounded source/output clock for one future deck. It converts a fixed output request plus playback rate into an exact bounded source-frame request, carries fractional timing deterministically between blocks, resets the clock on discontinuity and advances its source-consumption counters only after a successful processor call.
3. `src/core/TimeStretchSourceBridge.*` reads those exact requests from real `Clip` / `StreamCache` objects into scratch memory allocated by `prepare()`. It preserves a fractional source cursor with bounded interpolation, supports whole-track wrap, zero-pads unavailable pre-roll history before the start of a non-looping clip, records stream starvation/refill through the existing cache diagnostics, and fails closed without advancing transport if source data, sample-rate compatibility or processing is unavailable. A short prepared entry fade bounds prime/refill onset.
4. `src/core/TimeStretchDeviceBridge.*` is the explicit source-rate-to-device-rate boundary. It keeps a bounded preallocated stretched-output FIFO, converts it with a prepared 24-tap/128-phase band-limited resampler, separates audible transport from processor/FIFO prefetch, consumes caller-provided production fallback audio and exposes algorithm-latency metadata for future scheduling. Bypass, discontinuity and a changed rate/pitch invalidate prefetched research audio and require explicit off-callback re-prime before stretch resumes.

The device bridge remains **not connected to production `Engine::process()`**. It solves the hidden-clock problem for representative 44.1/48/96/192 kHz ratios inside the research path, but it does not yet claim fully compensated deck latency, seamless perceptual switching or safe live rate/pitch automation. The current research bridge deliberately accepts only source/device sample-rate ratios from 0.25 to 4.0 and device callback blocks up to 8192 frames; wider configurations must be qualified rather than silently accepted. The existing pitch-changing rate converter remains the production fallback.

## Automated evidence

With the prototype enabled, CTest adds:

- `time_stretch`: configuration/bounds, reported input/output latency, neutral 1.25x time-ratio pitch preservation, explicit +12-semitone pitch shift, reset and seek-preroll behavior, finite output.
- `time_stretch_deck`: exact bounded source-frame requests, fractional-clock carry, failure/no-advance behavior, discontinuity reset and pitch controls around the processor.
- `time_stretch_source_bridge`: real in-memory `Clip` reads, fractional-cursor transport, 1.25x pitch preservation, source-rate mismatch fallback, non-loop EOF, loop wrap, streamed cache starvation/no-advance and refill recovery.
- `time_stretch_device_bridge`: 44.1→48 kHz 1.25x pitch-lock transport, 96→48 kHz passband/stopband evidence, explicit production-fallback transport, bounded bypass transition, discontinuity fail-safe and algorithm-latency metadata.
- `time_stretch_realtime`, `time_stretch_deck_realtime`, `time_stretch_source_bridge_realtime` and `time_stretch_device_bridge_realtime`: warmed bounded processing contracts. Their measured windows must report zero heap allocations and deallocations.

Timing printed by the realtime tests is diagnostic only. Shared CI runners do not establish supported buffer sizes, hardware latency, CPU headroom or dropout-free live performance. The deterministic frequency/RMS fixtures are regression evidence, not proof of transparent processing on music.

## Integration gate

Do not replace or bypass the production fallback until all of the following have evidence:

1. Exact-head Linux sanitizer and Windows x64 CI pass with the prototype enabled.
2. Ratio, pitch, latency, reset/seek, source-bridge starvation/refill, device-rate conversion/fallback and zero-heap processing tests remain deterministic.
3. An Engine-facing opt-in integration uses the bounded source/device bridges without creating a dependency cycle and keeps disk I/O, decoding, blocking synchronization and allocation out of the callback.
4. Reported processor/SRC latency is applied to the deck/output scheduling model, and enable/disable, rate/pitch change, seek, loop, load, cache-starvation and processor-failure transitions crossfade/de-click against the production fallback under render tests.
5. Representative music-domain listening and CPU/callback-deadline/underrun measurements are performed on explicit Windows 11 hardware configurations.
6. Third-party license notices/source obligations remain complete for any distributed build that actually includes the dependency.

Until those gates pass, this is research infrastructure for M2, not production key lock.
