// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#include "Engine.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <stdexcept>

namespace broke {
static_assert(std::atomic<float>::is_always_lock_free);
static_assert(std::atomic<double>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);
static_assert(std::atomic<Clip*>::is_always_lock_free);
namespace {
float bounded(float x, float lo, float hi, float fallback = 0.0f) noexcept {
    return std::isfinite(x) ? std::clamp(x, lo, hi) : fallback;
}
float clean(float x) noexcept { return std::isfinite(x) ? x : 0.0f; }

float point(const std::vector<float>& data, std::int64_t index, bool loop) noexcept {
    const auto size = static_cast<std::int64_t>(data.size());
    if (size <= 0) return 0.0f;
    if (loop) {
        index %= size;
        if (index < 0) index += size;
    } else {
        index = std::clamp<std::int64_t>(index, 0, size - 1);
    }
    return clean(data[static_cast<std::size_t>(index)]);
}

// Four-point Catmull-Rom interpolation. It materially reduces the staircase/
// image energy of the original linear development resampler while keeping the
// callback allocation-free. A future key-lock/time-stretch stage remains separate.
float resample(const std::vector<float>& data, double cursor, bool loop) noexcept {
    if (data.empty() || !std::isfinite(cursor)) return 0.0f;
    if (data.size() < 4) {
        const auto i = static_cast<std::int64_t>(std::floor(cursor));
        const auto f = static_cast<float>(cursor - static_cast<double>(i));
        const float a = point(data, i, loop);
        const float b = point(data, i + 1, loop);
        return clean(a + (b - a) * f);
    }
    const auto i = static_cast<std::int64_t>(std::floor(cursor));
    const float t = static_cast<float>(cursor - static_cast<double>(i));
    const float p0 = point(data, i - 1, loop);
    const float p1 = point(data, i, loop);
    const float p2 = point(data, i + 1, loop);
    const float p3 = point(data, i + 2, loop);
    const float t2 = t * t;
    const float t3 = t2 * t;
    return clean(0.5f * ((2.0f * p1) + (-p0 + p2) * t
        + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
        + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3));
}
}
bool Clip::valid() const noexcept {
    return std::isfinite(sampleRate) && sampleRate >= 8000.0 && sampleRate <= 384000.0
        && !left.empty() && left.size() == right.size();
}
ClipMailbox::~ClipMailbox() {
    delete pending.load();
    delete retired.load();
    delete active;
}
void ClipMailbox::publish(std::unique_ptr<Clip> clip) noexcept {
    // An unconsumed request can be replaced and freed on the publishing thread.
    delete pending.exchange(clip.release(), std::memory_order_acq_rel);
}
void ClipMailbox::collect() noexcept {
    delete retired.exchange(nullptr, std::memory_order_acq_rel);
}
bool ClipMailbox::adopt() noexcept {
    // Backpressure: never overwrite a not-yet-collected retired clip.
    if (retired.load(std::memory_order_acquire) != nullptr) return false;
    auto* next = pending.exchange(nullptr, std::memory_order_acq_rel);
    if (next == nullptr) return false;
    retired.store(active, std::memory_order_release);
    active = next;
    return true;
}
void Engine::prepare(double rate) {
    if (!std::isfinite(rate) || rate < 8000.0 || rate > 192000.0)
        throw std::invalid_argument("Output sample rate must be 8-192 kHz");
    sampleRate = rate;
    lowCoeff = static_cast<float>(1.0 - std::exp(-2.0 * std::numbers::pi * 200.0 / rate));
    highCoeff = static_cast<float>(1.0 - std::exp(-2.0 * std::numbers::pi * 2400.0 / rate));
    smoothing = static_cast<float>(1.0 - std::exp(-1.0 / (rate * 0.005)));
    transitionSamples = std::max(1, static_cast<int>(std::lround(rate * 0.005)));
    masterSmooth = 0.0f;
    crossSmooth = 0.5f;
    for (auto& s : states) {
        for (auto& channel : s.delay) channel.assign(static_cast<std::size_t>(rate * 0.25), 0.0f);
        s.delayIndex = 0;
        s.bass.fill(0.0f);
        s.treble.fill(0.0f);
        s.lastProcessed.fill(0.0f);
        s.transitionFrom.fill(0.0f);
        s.gain = 0.0f;
        s.low = s.mid = s.high = 1.0f;
        s.echo = s.drive = 0.0f;
        s.transitionRemaining = 0;
        s.wasPlaying = false;
    }
}
bool Engine::submit(std::size_t deck, std::unique_ptr<Clip> clip) {
    if (deck >= deckCount || !clip || !clip->valid()) return false;
    clips[deck].publish(std::move(clip));
    return true;
}
void Engine::collectRetired() noexcept { for (auto& c : clips) c.collect(); }
void Engine::process(float* const* output, int channels, int frames) noexcept {
    if (output == nullptr || channels <= 0 || frames <= 0) return;
    for (int c = 0; c < channels; ++c)
        if (output[c] != nullptr) std::fill_n(output[c], frames, 0.0f);
    struct Block {
        const Clip* clip{};
        bool playing{}, loop{}, cue{};
        float gain{}, rate{}, low{}, mid{}, high{}, echo{}, drive{};
    };
    std::array<Block, deckCount> blocks{};
    std::array<float, deckCount> peaks{};
    for (std::size_t d = 0; d < deckCount; ++d) {
        auto& state = states[d];
        auto& control = controls[d];
        if (clips[d].adopt()) {
            state.cursor = 0.0;
            state.gain = 0.0f;
            state.bass.fill(0.0f);
            state.treble.fill(0.0f);
            state.lastProcessed.fill(0.0f);
            state.transitionFrom.fill(0.0f);
            state.transitionRemaining = 0;
            state.wasPlaying = false;
            for (auto& channel : state.delay) std::fill(channel.begin(), channel.end(), 0.0f);
            state.delayIndex = 0;
            // Loading a replacement track is deliberately stopped, never auto-played.
            control.playing.store(false, std::memory_order_relaxed);
        }
        auto& b = blocks[d];
        b.clip = clips[d].current();
        b.playing = control.playing.load(std::memory_order_relaxed);
        b.loop = control.loop.load(std::memory_order_relaxed);
        b.cue = control.headphone.load(std::memory_order_relaxed);
        b.gain = bounded(control.gain.load(), 0.0f, 1.5f);
        b.rate = bounded(control.rate.load(), 0.5f, 1.5f, 1.0f);
        b.low = bounded(control.low.load(), 0.0f, 2.0f, 1.0f);
        b.mid = bounded(control.mid.load(), 0.0f, 2.0f, 1.0f);
        b.high = bounded(control.high.load(), 0.0f, 2.0f, 1.0f);
        b.echo = bounded(control.echo.load(), 0.0f, 0.7f);
        b.drive = bounded(control.drive.load(), 0.0f, 6.0f);
        const auto seek = control.seek.exchange(-1.0, std::memory_order_relaxed);
        if (b.clip && std::isfinite(seek) && seek >= 0.0) {
            state.transitionFrom = state.lastProcessed;
            state.transitionRemaining = transitionSamples;
            state.cursor = std::clamp(seek, 0.0, 1.0) * static_cast<double>(b.clip->left.size() - 1);
        }
    }
    const float crossTarget = bounded(crossfader.load(), 0.0f, 1.0f, 0.5f);
    const float masterTarget = bounded(master.load(), 0.0f, 1.0f);
    const float cueLevel = bounded(headphoneLevel.load(), 0.0f, 1.0f);
    float peak = 0.0f;
    bool overload = false;
    for (int frame = 0; frame < frames; ++frame) {
        crossSmooth += smoothing * (crossTarget - crossSmooth);
        masterSmooth += smoothing * (masterTarget - masterSmooth);
        const float leftFade = std::cos(crossSmooth * std::numbers::pi_v<float> * 0.5f);
        const float rightFade = std::sin(crossSmooth * std::numbers::pi_v<float> * 0.5f);
        std::array<float, 2> mix{}, cueMix{};
        for (std::size_t d = 0; d < deckCount; ++d) {
            auto& s = states[d];
            auto& b = blocks[d];
            if (b.playing != s.wasPlaying) {
                s.transitionFrom = s.lastProcessed;
                s.transitionRemaining = transitionSamples;
                s.wasPlaying = b.playing;
            }
            s.gain += smoothing * (b.gain - s.gain);
            s.low += smoothing * (b.low - s.low);
            s.mid += smoothing * (b.mid - s.mid);
            s.high += smoothing * (b.high - s.high);
            s.echo += smoothing * (b.echo - s.echo);
            s.drive += smoothing * (b.drive - s.drive);
            std::array<float, 2> sample{};
            if (b.clip && b.playing) {
                const auto length = static_cast<double>(b.clip->left.size());
                if (s.cursor >= length) {
                    if (b.loop) s.cursor = std::fmod(s.cursor, length);
                    else {
                        b.playing = false;
                        controls[d].playing.store(false, std::memory_order_relaxed);
                        if (s.wasPlaying) {
                            s.transitionFrom = s.lastProcessed;
                            s.transitionRemaining = transitionSamples;
                            s.wasPlaying = false;
                        }
                    }
                }
                if (b.playing) {
                    sample[0] = resample(b.clip->left, s.cursor, b.loop);
                    sample[1] = resample(b.clip->right, s.cursor, b.loop);
                    s.cursor += b.clip->sampleRate / sampleRate * b.rate;
                }
            }
            const float transitionMix = s.transitionRemaining > 0
                ? 1.0f - static_cast<float>(s.transitionRemaining) / static_cast<float>(transitionSamples)
                : 1.0f;
            for (std::size_t c = 0; c < 2; ++c) {
                const float in = bounded(sample[c], -16.0f, 16.0f);
                s.bass[c] += lowCoeff * (in - s.bass[c]);
                s.treble[c] += highCoeff * (in - s.treble[c]);
                float x = s.bass[c] * s.low + (s.treble[c] - s.bass[c]) * s.mid + (in - s.treble[c]) * s.high;
                if (s.drive > 0.001f) x = std::tanh(x * (1.0f + s.drive)) / std::tanh(1.0f + s.drive);
                if (!s.delay[c].empty()) {
                    const float delayed = s.delay[c][s.delayIndex];
                    s.delay[c][s.delayIndex] = clean(x + delayed * 0.35f);
                    x = x * (1.0f - s.echo) + delayed * s.echo;
                }
                x = clean(x);
                if (s.transitionRemaining > 0)
                    x = s.transitionFrom[c] * (1.0f - transitionMix) + x * transitionMix;
                s.lastProcessed[c] = x;
                if (b.cue) cueMix[c] += x * cueLevel; // pre-fader, post-EQ/FX
                x *= s.gain;
                peaks[d] = std::max(peaks[d], std::abs(x));
                mix[c] += x * ((d % 2 == 0) ? leftFade : rightFade);
            }
            if (s.transitionRemaining > 0) --s.transitionRemaining;
            if (!s.delay[0].empty()) s.delayIndex = (s.delayIndex + 1) % s.delay[0].size();
        }
        for (int c = 0; c < 2; ++c) {
            const float x = clean(mix[static_cast<std::size_t>(c)] * masterSmooth);
            overload = overload || std::abs(x) > 0.98f;
            peak = std::max(peak, std::abs(x));
            if (c < channels && output[c]) output[c][frame] = std::clamp(x, -0.98f, 0.98f);
            // Never fold headphone cue into the main stereo pair.
            if (channels >= 4 && output[c + 2])
                output[c + 2][frame] = bounded(cueMix[static_cast<std::size_t>(c)], -0.98f, 0.98f);
        }
    }
    for (std::size_t d = 0; d < deckCount; ++d) {
        const auto* clip = blocks[d].clip;
        meters[d].duration.store(clip ? static_cast<double>(clip->left.size()) / clip->sampleRate : 0.0);
        meters[d].position.store(clip ? std::min(states[d].cursor, static_cast<double>(clip->left.size())) / clip->sampleRate : 0.0);
        meters[d].peak.store(peaks[d]);
    }
    masterPeak.store(peak);
    clipped.store(overload);
}
} // namespace broke
