# Architecture

## Threads and ownership

The message thread owns components, file selection, control edits and clip publication. A single decoder worker performs file I/O and decoding; at most one load per deck is queued. Closing the app cancels decoding, stops audio and joins the worker before destruction.

The core owns four immutable planar stereo clips. Publication uses one pending atomic pointer per deck. The audio callback adopts a pointer only when the previous retired pointer has been collected; this provides backpressure instead of overwriting ownership. The message thread reclaims retired clips. Replacing an unconsumed pending clip also deletes it on the message thread. The callback never frees a clip or touches the filesystem.

Atomics used in the callback are required to be always lock-free at compile time. Control values are sampled per block. Channel/master gain and crossfader are smoothed; EQ, effect and transport automation are **not yet fully smoothed**. Audio must be stopped before `prepare()` or engine destruction.

## Signal path

Per deck: decoded clip -> linear variable-rate interpolation -> basic split-band EQ -> saturation -> fixed 250 ms feedback echo -> pre-fader headphone tap -> channel gain -> crossfader group -> master gain -> hard sample ceiling.

A/C belong to the left crossfader group and B/D to the right. Cue reaches output indices 2 and 3 only if at least four channels are available. It is never automatically mixed into the master. Output ceiling protection is not a look-ahead or true-peak limiter.

## Current boundaries

The core is independent of JUCE and tested offline. JUCE is the device/format/GUI adapter. No SQLite, Rubber Band, VST3 host, model runtime or controller SDK is silently pulled in for a feature that does not exist yet.

Each decoded track is capped at 256 MiB stereo float data; replacement/retirement can temporarily retain more than one clip per deck. Streaming with bounded read-ahead buffers must replace this approach before declaring large-library/live readiness. The current resampler is linear, not the final high-quality anti-aliasing/key-lock path. Track adoption clears preallocated delay buffers, a bounded operation whose worst-case callback cost still needs measurement.

## Planned extension boundaries

Analysis/cache workers feed immutable beat-grid/metadata results. Effect racks will have prepared fixed-capacity graph snapshots rather than GUI-owned objects accessed by audio. Plugin scanning is out-of-process; runtime isolation and recovery require latency, routing and license evaluation. Optional AI separation must not be a dependency of ordinary playback. No permanent hardware or universal plugin compatibility claims without a test matrix.
