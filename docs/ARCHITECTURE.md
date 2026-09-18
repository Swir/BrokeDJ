# Architecture

## Threads and ownership

The message thread owns components, file selection, control edits and clip publication. A single decoder worker performs file I/O and decoding; at most one load per deck is queued. Closing the app cancels decoding, stops audio and joins the worker before destruction.

The core owns four immutable planar stereo clips. Publication uses one pending atomic pointer per deck. The audio callback adopts a pointer only when the previous retired pointer has been collected; this provides backpressure instead of overwriting ownership. The message thread reclaims retired clips. Replacing an unconsumed pending clip also deletes it on the message thread. The callback never frees a clip or touches the filesystem.

Atomics used in the callback are required to be always lock-free at compile time. Control values are sampled per block. Channel/master gain and crossfader are smoothed. EQ, echo and drive targets now use the same short one-pole smoothing time constant. Play/pause and seek discontinuities use a prepared fixed-length transition window derived from the output sample rate. Audio must be stopped before `prepare()` or engine destruction.

## Signal path

Per deck: decoded immutable clip -> four-point Catmull-Rom variable-rate interpolation -> basic split-band EQ -> saturation -> fixed 250 ms feedback echo -> short transport transition blend -> pre-fader headphone tap -> channel gain -> crossfader group -> master gain -> hard sample ceiling.

A/C belong to the left crossfader group and B/D to the right. Cue reaches output indices 2 and 3 only if at least four channels are available. It is never automatically mixed into the master. Output ceiling protection is not a look-ahead or true-peak limiter.

The Catmull-Rom path is an incremental improvement over the original linear development resampler. It remains pitch-changing rate conversion and does not perform phase-vocoder/time-domain stretching, anti-aliasing across every transposition, or key lock. Those are separate M2 engineering gates.

## Real-time callback boundaries

The audio callback performs bounded arithmetic, atomic loads/stores, preallocated delay-buffer access and immutable clip reads. It does not perform file/network I/O, decoding, plugin scanning, allocation/free of clip objects or blocking mutex acquisition.

Track adoption clears already allocated delay buffers. This is bounded but can still be a measurable callback spike, so its worst-case cost needs measurement before live qualification. A later design may move more reset work outside the callback if profiling justifies it.

## Current storage / decode boundary

The core is independent of JUCE and tested offline. JUCE is the device/format/GUI adapter. No SQLite, Rubber Band, VST3 host, model runtime or controller SDK is silently pulled in for a feature that does not exist yet.

Each decoded track is currently capped at 256 MiB stereo float data; replacement/retirement can temporarily retain more than one clip per deck. Streaming with bounded read-ahead buffers must replace this approach before declaring large-library/live readiness. Decoder work and waveform peak generation remain off the audio thread.

## Quality validation boundary

Deterministic tests cover transport transition behavior and finite output during aggressive EQ/FX control changes in addition to the original playback, mixer, cue, looping, clipping and concurrent handoff checks. Automated render checks support implementation confidence but do not replace reviewed listening tests on representative material and real output devices.

## Planned extension boundaries

Analysis/cache workers feed immutable beat-grid/metadata results. Effect racks will have prepared fixed-capacity graph snapshots rather than GUI-owned objects accessed by audio. Plugin scanning is out-of-process; runtime isolation and recovery require latency, routing and license evaluation. Optional AI separation must not be a dependency of ordinary playback. No permanent hardware or universal plugin compatibility claims without a test matrix.
