# Architecture

## Threads and ownership

The message thread owns components, file selection, control edits and clip publication. Import/preview preparation runs off the audio thread. Small files are decoded by the loader worker; large files transfer an `AudioFormatReader` into a dedicated background read-ahead thread. Closing/replacing a streamed clip destroys that source only after the audio thread has retired the clip and the non-audio collector reclaims it.

The core owns four immutable clip descriptors. Publication uses one pending atomic pointer per deck. The audio callback adopts a pointer only when the previous retired pointer has been collected; this provides backpressure instead of overwriting ownership. The message thread reclaims retired clips. Replacing an unconsumed pending clip also deletes it on the publishing thread. The callback never frees a clip or touches the filesystem.

Atomics used in the callback are required to be always lock-free at compile time. Control values are sampled per block. Channel/master gain and crossfader are smoothed. EQ, echo, drive, playback rate and cue targets use short one-pole smoothing. Play/pause, seek and loop discontinuities use a prepared fixed-length transition window derived from the output sample rate. Audio must be stopped before `prepare()` or engine destruction.

## Signal path

Per deck: in-memory clip **or** bounded stream cache -> four-point Catmull-Rom variable-rate interpolation -> basic split-band EQ -> saturation -> fixed 250 ms feedback echo -> short transport transition blend -> pre-fader headphone tap -> channel gain -> crossfader group -> master gain -> hard sample ceiling.

A/C belong to the left crossfader group and B/D to the right. Cue reaches output indices 2 and 3 only if at least four channels are available. It is never automatically mixed into the master. Output ceiling protection is not a look-ahead or true-peak limiter.

The Catmull-Rom path is pitch-changing rate conversion and does not perform phase-vocoder/time-domain stretching, anti-aliasing across every transposition, or key lock. Those remain M2 engineering gates.

## Real-time callback boundaries

The audio callback performs bounded arithmetic, atomic loads/stores, preallocated delay-buffer access and immutable clip/cache reads. It does not perform file/network I/O, decoding, plugin scanning, allocation/free of clip objects or blocking mutex acquisition.

Track adoption clears already allocated delay buffers. This is bounded but can still be a measurable callback spike, so its worst-case cost needs measurement before live qualification.

## Bounded long-track streaming

Small tracks whose decoded stereo float payload is at most 64 MiB use the simple in-memory path. Larger supported local files use `StreamCache`: 32 fixed slots of 4096 stereo frames. Sample cells and publication metadata are atomic so the audio callback never locks while a background reader refills a slot.

The reader primes the first region, then follows a last-request-wins frame hint from transport/seek. The exact requested chunk is refilled first, forward read-ahead follows, and one previous chunk is then considered for interpolation support. Chunks outside the file are skipped before decode work so the worker cannot spin merely because its forward window extends beyond EOF. A missing cache cell returns silence and requests that source region; the callback never waits for disk/decoder work. Sparse waveform preview generation reads bounded windows off the audio thread instead of allocating the full decoded track.

A non-audio `StreamCacheDiagnostics` snapshot reports the requested frame/chunk, whether that exact requested region is resident and how many chunks are resident in a bounded forward window. This is intentionally cache-readiness observability, not a hardware underrun counter or a live-readiness signal.

This architecture bounds sample-cache memory independently of track duration and retires the previous 256 MiB full-decode ceiling for the streaming path. It is still pre-alpha: no claim is made that every disk/codec/seek sequence is dropout-free. Cache-refill onset smoothing, event/history diagnostics, long-loop boundaries on real files, slow-storage stress and cancellation of a decoder blocked inside OS/file I/O remain hardening work.

## Quality validation boundary

Deterministic tests cover transport transitions, finite output during aggressive controls and stream-cache publication/miss/seek behavior. The streaming fixture can model a 90-minute track without allocating full-track audio, exercise repeated distant seeks, a prepared whole-track loop edge and starvation/refill snapshots. Automated render checks support implementation confidence but do not replace reviewed listening tests on representative material and real output devices.

## Planned extension boundaries

Analysis/cache workers feed immutable beat-grid/metadata results. Effect racks will have prepared fixed-capacity graph snapshots rather than GUI-owned objects accessed by audio. Plugin scanning is out-of-process; runtime isolation and recovery require latency, routing and license evaluation. Optional AI separation must not be a dependency of ordinary playback. No permanent hardware or universal plugin compatibility claims without a test matrix.
