# Architecture

## Threads and ownership

The message thread owns components, file selection, control edits and clip publication. Import/preview preparation runs off the audio thread. Small files are decoded by the loader worker; large files transfer an `AudioFormatReader` into a dedicated background read-ahead thread. Closing/replacing a streamed clip destroys that source only after the audio thread has retired the clip and the non-audio collector reclaims it.

The core owns four immutable clip descriptors. Publication uses one pending atomic pointer per deck. The audio callback adopts a pointer only when the previous retired pointer has been collected; this provides backpressure instead of overwriting ownership. The message thread reclaims retired clips. Replacing an unconsumed pending clip also deletes it on the publishing thread. The callback never frees a clip or touches the filesystem.

Atomics used in the callback are required to be always lock-free at compile time. Control values are sampled per block. Channel/master gain and crossfader are smoothed. EQ, echo, drive, playback rate and cue targets use short one-pole smoothing. Play/pause, seek, loop and streamed starvation/refill discontinuities use a prepared fixed-length transition window derived from the output sample rate. Audio must be stopped before `prepare()` or engine destruction.

## Signal path

Per deck: in-memory clip **or** bounded stream cache -> hybrid variable-rate converter -> basic split-band EQ -> saturation -> fixed 250 ms feedback echo -> short transport/cache transition blend -> pre-fader headphone tap -> channel gain -> crossfader group -> master gain -> smooth bounded output safety curve.

The rate converter uses four-point Catmull-Rom interpolation when the effective source step does not require downsampling. When the step exceeds one source frame per output sample, it switches to a prepared 24-tap Blackman-windowed sinc kernel selected from a finite cutoff/phase lookup bank. The bank is allocated and normalized by `prepare()` while audio is stopped; the callback only performs bounded lookup and multiply-accumulate work. Its cutoff follows the source/output/rate step so energy above the output Nyquist region is reduced before decimation. This remains pitch-changing resampling, not time-stretch or key lock.

### Optional M2 time-stretch research path

The opt-in research path is deliberately separate from ordinary `Engine::process()` and is compiled only when `BROKEDJ_BUILD_TIMESTRETCH_PROTOTYPE=ON`.

`TimeStretchPrototype` wraps the pinned Signalsmith processor and owns processor configuration/latency reporting. `TimeStretchDeckAdapter` adds a bounded deck-side source/output clock with deterministic fractional carry and discontinuity reset. `TimeStretchSourceBridge` gathers exact bounded requests from BrokeDJ `Clip` or `StreamCache` objects into prepared scratch storage, preserves fractional source cursor alignment, supports loop wrapping, fails closed without transport advance on cache miss/EOF/source-rate mismatch, and reuses the existing starvation/refill diagnostics. Prime/refill entry uses a short prepared fade rather than an abrupt full-scale onset.

`TimeStretchDeviceBridge` adds the explicit output-domain boundary above that source bridge. A bounded preallocated FIFO holds stretched source-rate output; a prepared 24-tap, 128-phase Blackman-windowed sinc bank converts it to the configured device rate without allocating in render. Audible transport is advanced from device duration, source/device rate ratio and playback rate instead of from FIFO prefetch depth. The bridge accepts caller-supplied production fallback samples, reports which path rendered, and fails to fallback rather than emitting stale research data. Bypass, transport discontinuity and changed rate/pitch invalidate prefetched research output and require an explicit off-callback `prime()` before stretch can resume.

The current research bridge qualifies source/device ratios from 0.25 to 4.0 and device blocks up to 8192 frames. Its reported device-output latency combines processor metadata with the finite SRC history for future scheduling; this is algorithm metadata, **not** measured hardware latency and not yet full deck/output latency compensation. The `brokedj_core` target still does not depend on the optional research library, avoiding a dependency cycle and ensuring ordinary playback remains buildable without Signalsmith. See [`TIME_STRETCH_PROTOTYPE.md`](TIME_STRETCH_PROTOTYPE.md).

Before this path can supplement production playback, an opt-in Engine-facing integration must preserve the current realtime contract while applying reported latency to deck scheduling, preparing/re-priming outside the callback and rendering the current production converter as the deterministic parallel fallback across enable/bypass, rate/pitch changes, seek/loop/load, cache starvation/refill and processor failure. Windows 11 music-domain listening plus explicit device CPU/callback-deadline/underrun evidence remain mandatory before user-facing key lock.

A/C belong to the left crossfader group and B/D to the right. Cue reaches output indices 2 and 3 only if at least four channels are available. It is never automatically mixed into the master. Output safety protection is not a look-ahead or true-peak limiter.

## Real-time callback boundaries

The audio callback performs bounded arithmetic, atomic loads/stores, immutable precomputed resampler-kernel reads, preallocated delay-buffer access and immutable clip/cache reads. It does not perform file/network I/O, decoding, plugin scanning, kernel generation, allocation/free of clip objects or blocking mutex acquisition.

Track adoption clears already allocated delay buffers. This is bounded but can still be a measurable callback spike, so its worst-case cost needs measurement before live qualification.

Stream-cache diagnostics use lock-free counters only. A failed cache tap increments a read-miss counter and stores the last missed frame. The engine collapses repeated failed taps into a single starvation episode and records one refill event when a complete interpolation frame becomes readable again. No audio-thread logging, heap activity or blocking synchronization is introduced.

The JUCE-independent `realtime_contract` test instruments heap allocation in its own executable and stresses 1,200 callback blocks with in-memory and streamed decks plus transport and control automation. The measured window must observe zero allocations and zero deallocations. It also reports elapsed callback cost as diagnostic evidence, but shared CI runner timing is intentionally not a release threshold and does not replace device-specific callback-deadline/underrun measurements.

The optional time-stretch realtime contracts separately instrument warmed processor, deck-clock, source-bridge and device-bridge render windows. The device bridge owns its FIFO, SRC kernels and scratch storage before the measured window. Passing these tests is a heap/bounds contract only; it does not make the research path part of the production callback.

## Bounded long-track streaming

Small tracks whose decoded stereo float payload is at most 64 MiB use the simple in-memory path. Larger supported local files use `StreamCache`: 32 fixed slots of 4096 stereo frames. Sample cells and publication metadata are atomic so the audio callback never locks while a background reader refills a slot.

The reader primes the first region, then follows a last-request-wins frame hint from transport/seek. After a new request it fills the exact requested chunk first, then the immediate previous and next chunks before deeper forward read-ahead. This ordering supports interpolation kernels that can need samples on either side of the cursor and reduces avoidable starvation at chunk boundaries after a seek. Chunks outside the file are skipped before decode work so the worker cannot spin merely because its forward window extends beyond EOF. A missing cache cell returns silence and requests that source region; the callback never waits for disk/decoder work. Sparse waveform preview generation reads bounded windows off the audio thread instead of allocating the full decoded track.

A streamed output frame is considered readable only when every interpolation tap needed by both stereo channels is available. Partial tap sets are rejected as a unit rather than mixing cached values with implicit zeros. When playback enters starvation, the engine fades from the last processed sample toward bounded silence; when a fully readable interpolation frame returns, it fades back in using the same short transition window. This reduces abrupt refill edges but is not a claim of inaudible recovery for every codec/storage path.

A non-audio `StreamCacheDiagnostics` snapshot reports the requested frame/chunk, whether that exact requested region is resident, bounded forward coverage, total failed cache reads, starvation/refill episode counts, current starvation state and the most recent missed frame. These are cache/playback observability signals, not hardware underrun counters or live-readiness evidence.

This architecture bounds sample-cache memory independently of track duration and retires the previous 256 MiB full-decode ceiling for the streaming path. It is still pre-alpha: no claim is made that every disk/codec/seek sequence is dropout-free. Real-codec long-loop boundaries, slow-storage stress, reviewed listening and cancellation of a decoder blocked inside OS/file I/O remain hardening work.

## Musical analysis and manual grid ownership

BPM/grid and musical-key analysis run only on the dedicated analysis worker after a deck becomes playable. The worker performs one bounded sequential decode pass for both analyzers, then publishes immutable result snapshots to the message thread. Detector cache records and manual-grid override records are separate: both bind to source size/modification identity and omit the raw source path/name from payload data.

`BeatGrid` remains JUCE-independent and owns validated beat↔time mapping plus bounded variable-tempo segments. The message thread owns each deck's currently displayed detected grid and active grid. `TrackBeatGridOverrideStore` is consulted on the analysis worker; a valid matching manual override wins over detector output. Compact UI edits copy and transactionally validate the current grid, then update the message-thread snapshot immediately while persistence is serialized through the analysis worker. A per-deck generation counter prevents stale completion messages from replacing status after a newer edit/reset/load. Reset immediately restores the detected grid and queues override deletion. None of these operations run in `Engine::process()`.

The waveform remains an amplitude preview. Beat lines are a bounded rendering overlay derived from the validated active grid, with display stride increased when needed so painting cannot iterate an unbounded number of beats. A brighter overlay identifies a manual grid. The deck exposes fast beat-zero/segment-0 BPM controls plus a native reviewed tempo-map dialog. Later boundaries can be added at the playhead, selected, moved, replaced or removed with bounded Undo/Redo; accepted snapshots flow through `PerformanceDeckOwner` and persist asynchronously outside `Engine::process()`.

`brokedj_analysis_validator` is a local developer tool built alongside the native app. It reads a manifest outside the repository, invokes the uncached offline detector path on user-owned/licensed audio and reports stable manifest IDs plus aggregate BPM/key evidence without printing source paths. It is deliberately not an automatic CTest because no reference music is redistributed with BrokeDJ. See [`ANALYSIS_VALIDATION.md`](ANALYSIS_VALIDATION.md).

## Continuous reviewed-grid Sync ownership

Continuous Sync remains a control-thread feature rather than an audio-callback beat scheduler. Enabling a follower first uses the existing bounded reviewed-grid alignment. While both master and follower are playing, a 5 Hz message-thread service asks `PerformanceDeckOwner` to re-evaluate local variable-tempo BPM and beat phase against the selected master. The owner only writes a new rate when it differs by more than a small epsilon, suppresses phase seeks inside an 0.08-beat deadband, and rejects corrections beyond a 0.35-beat envelope or outside the qualified 0.8–1.2 relative-rate range.

Reviewed Beat Loop or Reverse/Slip ownership on either deck fails maintenance closed so Sync cannot fight another transport workflow. Native Jog/Scratch activity is observed by the UI integration and temporarily suspends maintenance while the platter is touched; release resumes the existing lock rather than changing callback topology. Replacing the master clears follower locks, and replacing a follower clip clears only that deck's lock. Optional key-lock research is notified of any accepted rate/phase maintenance so it can continue to fall back conservatively. These rules are deterministic software behavior only; MIDI/controller timing, physical latency and reviewed music-domain accuracy still require separate qualification.

## Quality validation boundary

Deterministic tests cover transport transitions, finite output during aggressive controls, stream-cache publication/miss/seek behavior, starvation episode accounting and refill-onset smoothing. The streaming fixture can model a 90-minute track without allocating full-track audio, exercise repeated distant seeks, a prepared whole-track loop edge and intentional starvation/refill recovery.

The `render_metrics` target adds reproducible numeric evidence for the pitch-changing rate converter and transition system: low-frequency rate/frequency error, residual RMS ratio, DC, output level/peak and maximum adjacent-sample transition delta are gated on deterministic fixtures. A separate 1.5x test compares an 8 kHz passband tone with a 19 kHz source that would fold into the audible band without pre-decimation filtering; it gates stopband RMS and the stopband/passband ratio. These are regression baselines for the implementation, not proof of perceptual transparency, ideal reconstruction, professional key lock or inaudibility on arbitrary music.

The optional device-bridge fixture separately checks key-locked 44.1→48 kHz transport/frequency behavior, 96→48 kHz passband versus alias-prone stopband output, fallback transport preservation, bounded transitions and zero-heap warmed rendering. Those tests remain research evidence and do not replace reviewed listening.

Automated render checks support implementation confidence but do not replace reviewed listening tests on representative material and real output devices. The current finite polyphase/windowed-sinc banks are engineering improvements over unfiltered paths, not a claim that no stronger production resampler will ever be justified.

## Planned extension boundaries

Corrected beat grids will feed later sync, quantized hotcue and beat-loop scheduling through immutable/prepared snapshots rather than adding filesystem or analysis work to the audio callback. Effect racks will have prepared fixed-capacity graph snapshots rather than GUI-owned objects accessed by audio. Plugin scanning is out-of-process; runtime isolation and recovery require latency, routing and license evaluation. Optional AI separation must not be a dependency of ordinary playback. No permanent hardware or universal plugin compatibility claims without a test matrix.
