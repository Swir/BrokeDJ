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
static_assert(std::atomic<std::uint8_t>::is_always_lock_free);
static_assert(std::atomic<std::size_t>::is_always_lock_free);
static_assert(std::atomic<std::int64_t>::is_always_lock_free);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<Clip*>::is_always_lock_free);
namespace {
float bounded(float x, float lo, float hi, float fallback = 0.0f) noexcept {
    return std::isfinite(x) ? std::clamp(x, lo, hi) : fallback;
}
float clean(float x) noexcept { return std::isfinite(x) ? x : 0.0f; }

// Allocation-free output protection with a unity-slope knee. Signals at or
// below -0.92 dBFS (0.90 linear) pass unchanged; only the top 0.8 dB is
// progressively compressed toward the 0.98 ceiling. This is deliberately a
// safety curve, not a transparent look-ahead limiter or a replacement for
// proper gain staging.
float protectOutput(float x) noexcept {
    x = clean(x);
    constexpr float knee = 0.90f;
    constexpr float ceiling = 0.98f;
    constexpr float span = ceiling - knee;
    const float magnitude = std::abs(x);
    if (magnitude <= knee) return x;
    const float shaped = knee + span * std::tanh((magnitude - knee) / span);
    return std::copysign(std::min(shaped, ceiling), x);
}

struct ReadPoint final {
    float value = 0.0f;
    bool ready = false;
};

ReadPoint point(const Clip& clip, int channel, std::int64_t index, bool loop) noexcept {
    const auto size = clip.frames();
    if (size <= 0) return {};
    if (loop) {
        index %= size;
        if (index < 0) index += size;
    } else {
        index = std::clamp<std::int64_t>(index, 0, size - 1);
    }
    if (clip.stream) {
        float value = 0.0f;
        const bool ready = clip.stream->trySample(channel, index, value);
        return {clean(value), ready};
    }
    const auto& data = channel == 0 ? clip.left : clip.right;
    return {clean(data[static_cast<std::size_t>(index)]), true};
}

// Catmull-Rom remains the low-cost path when no anti-alias low-pass is needed.
// When the effective source step exceeds one frame/output-sample, Engine passes
// a precomputed normalized Blackman-windowed sinc kernel. The kernel is built
// in prepare(), never in the realtime callback.
ReadPoint resample(const Clip& clip, int channel, double cursor, bool loop,
                   const float* bandlimitedKernel) noexcept {
    if (clip.frames() <= 0 || !std::isfinite(cursor)) return {};
    if (bandlimitedKernel != nullptr && clip.frames() >= static_cast<std::int64_t>(resamplerTaps)) {
        constexpr int firstTap = 1 - static_cast<int>(resamplerTaps / 2);
        const auto centre = static_cast<std::int64_t>(std::floor(cursor));
        float value = 0.0f;
        for (std::size_t tap = 0; tap < resamplerTaps; ++tap) {
            const auto sample = point(clip, channel,
                centre + static_cast<std::int64_t>(firstTap + static_cast<int>(tap)), loop);
            if (!sample.ready) return {};
            value += sample.value * bandlimitedKernel[tap];
        }
        return {clean(value), true};
    }
    if (clip.frames() < 4) {
        const auto i = static_cast<std::int64_t>(std::floor(cursor));
        const auto f = static_cast<float>(cursor - static_cast<double>(i));
        const auto a = point(clip, channel, i, loop);
        const auto b = point(clip, channel, i + 1, loop);
        if (!a.ready || !b.ready) return {};
        return {clean(a.value + (b.value - a.value) * f), true};
    }
    const auto i = static_cast<std::int64_t>(std::floor(cursor));
    const float t = static_cast<float>(cursor - static_cast<double>(i));
    const auto p0 = point(clip, channel, i - 1, loop);
    const auto p1 = point(clip, channel, i, loop);
    const auto p2 = point(clip, channel, i + 1, loop);
    const auto p3 = point(clip, channel, i + 2, loop);
    if (!p0.ready || !p1.ready || !p2.ready || !p3.ready) return {};
    const float t2 = t * t;
    const float t3 = t2 * t;
    return {clean(0.5f * ((2.0f * p1.value) + (-p0.value + p2.value) * t
        + (2.0f * p0.value - 5.0f * p1.value + 4.0f * p2.value - p3.value) * t2
        + (-p0.value + 3.0f * p1.value - 3.0f * p2.value + p3.value) * t3)), true};
}
}

StreamCache::Slot::Slot()
    : left(std::make_unique<std::atomic<float>[]>(chunkFrames)),
      right(std::make_unique<std::atomic<float>[]>(chunkFrames)) {
    for (std::size_t i = 0; i < chunkFrames; ++i) {
        left[i].store(0.0f, std::memory_order_relaxed);
        right[i].store(0.0f, std::memory_order_relaxed);
    }
}

StreamCache::StreamCache(std::int64_t totalFrames) : total(std::max<std::int64_t>(0, totalFrames)) {}

std::size_t StreamCache::slotFor(std::int64_t chunkIndex) noexcept {
    return static_cast<std::size_t>(chunkIndex % static_cast<std::int64_t>(slotCount));
}

bool StreamCache::hasChunk(std::int64_t chunkIndex) const noexcept {
    if (chunkIndex < 0) return false;
    const auto& slot = slots[slotFor(chunkIndex)];
    return slot.chunk.load(std::memory_order_acquire) == chunkIndex
        && slot.validFrames.load(std::memory_order_acquire) > 0;
}

void StreamCache::publishChunk(std::int64_t chunkIndex, const float* leftData, const float* rightData,
                               std::size_t frames) noexcept {
    if (chunkIndex < 0 || leftData == nullptr || rightData == nullptr || frames == 0) return;
    const auto start = chunkIndex * static_cast<std::int64_t>(chunkFrames);
    if (start < 0 || start >= total) return;
    const auto remaining = static_cast<std::size_t>(std::min<std::int64_t>(
        static_cast<std::int64_t>(chunkFrames), total - start));
    const auto count = std::min(frames, remaining);
    auto& slot = slots[slotFor(chunkIndex)];
    slot.chunk.store(-1, std::memory_order_release);
    slot.validFrames.store(0, std::memory_order_release);
    for (std::size_t i = 0; i < count; ++i) {
        slot.left[i].store(clean(leftData[i]), std::memory_order_relaxed);
        slot.right[i].store(clean(rightData[i]), std::memory_order_relaxed);
    }
    slot.validFrames.store(count, std::memory_order_release);
    slot.chunk.store(chunkIndex, std::memory_order_release);
}

void StreamCache::noteReadMiss(std::int64_t frame) const noexcept {
    lastMissFrame.store(frame, std::memory_order_relaxed);
    readMisses.fetch_add(1, std::memory_order_relaxed);
}

bool StreamCache::trySample(int channel, std::int64_t frame, float& value) const noexcept {
    value = 0.0f;
    if (frame < 0 || frame >= total || (channel != 0 && channel != 1)) return false;
    const auto chunkIndex = frame / static_cast<std::int64_t>(chunkFrames);
    const auto offset = static_cast<std::size_t>(frame % static_cast<std::int64_t>(chunkFrames));
    const auto& slot = slots[slotFor(chunkIndex)];
    if (slot.chunk.load(std::memory_order_acquire) != chunkIndex) {
        noteReadMiss(frame);
        request(frame);
        return false;
    }
    const auto valid = slot.validFrames.load(std::memory_order_acquire);
    if (offset >= valid) {
        noteReadMiss(frame);
        request(frame);
        return false;
    }
    value = (channel == 0 ? slot.left[offset] : slot.right[offset]).load(std::memory_order_relaxed);
    if (slot.chunk.load(std::memory_order_acquire) != chunkIndex) {
        value = 0.0f;
        noteReadMiss(frame);
        request(frame);
        return false;
    }
    value = clean(value);
    return true;
}

float StreamCache::sample(int channel, std::int64_t frame) const noexcept {
    float value = 0.0f;
    static_cast<void>(trySample(channel, frame, value));
    return value;
}

void StreamCache::request(std::int64_t frame) const noexcept {
    if (total <= 0) return;
    requested.store(std::clamp<std::int64_t>(frame, 0, total - 1), std::memory_order_relaxed);
}

void StreamCache::noteStarvation(std::int64_t frame) const noexcept {
    if (total <= 0) return;
    lastMissFrame.store(std::clamp<std::int64_t>(frame, 0, total - 1), std::memory_order_relaxed);
    if (!starving.exchange(true, std::memory_order_acq_rel))
        starvationEvents.fetch_add(1, std::memory_order_relaxed);
}

void StreamCache::noteRefill() const noexcept {
    if (starving.exchange(false, std::memory_order_acq_rel))
        refillEvents.fetch_add(1, std::memory_order_relaxed);
}

std::int64_t Clip::frames() const noexcept {
    if (stream) return frameCount > 0 ? frameCount : stream->totalFrames();
    return static_cast<std::int64_t>(left.size());
}

bool Clip::valid() const noexcept {
    if (!std::isfinite(sampleRate) || sampleRate < 8000.0 || sampleRate > 384000.0) return false;
    if (stream)
        return frameCount > 0 && frameCount == stream->totalFrames();
    return !left.empty() && left.size() == right.size();
}

ClipMailbox::~ClipMailbox() {
    delete pending.load();
    delete retired.load();
    delete active;
}
void ClipMailbox::publish(std::unique_ptr<Clip> clip) noexcept {
    // A later publication supersedes an unconsumed clear request. The single
    // non-audio publisher rule keeps this ordering deterministic without a lock.
    clearRequested.store(false, std::memory_order_release);
    delete pending.exchange(clip.release(), std::memory_order_acq_rel);
}
void ClipMailbox::requestClear() noexcept {
    // Removing a not-yet-adopted source is non-realtime work and can delete it
    // here. The active source is never deleted here because it is audio-owned.
    delete pending.exchange(nullptr, std::memory_order_acq_rel);
    clearRequested.store(true, std::memory_order_release);
}
void ClipMailbox::collect() noexcept {
    delete retired.exchange(nullptr, std::memory_order_acq_rel);
}
bool ClipMailbox::adopt() noexcept {
    if (retired.load(std::memory_order_acquire) != nullptr) return false;
    if (clearRequested.exchange(false, std::memory_order_acq_rel)) {
        retired.store(active, std::memory_order_release);
        active = nullptr;
        return true;
    }
    auto* next = pending.exchange(nullptr, std::memory_order_acq_rel);
    if (next == nullptr) return false;
    retired.store(active, std::memory_order_release);
    active = next;
    return true;
}

void Engine::prepareResamplerKernels() {
    const auto weightCount = resamplerCutoffBins * resamplerPhases * resamplerTaps;
    resamplerKernels.assign(weightCount, 0.0f);
    constexpr int firstTap = 1 - static_cast<int>(resamplerTaps / 2);
    const double radius = static_cast<double>(resamplerTaps) * 0.5;
    const double cutoffSpan = 1.0 - static_cast<double>(resamplerMinCutoff);
    for (std::size_t bin = 0; bin < resamplerCutoffBins; ++bin) {
        const double cutoff = static_cast<double>(resamplerMinCutoff)
            + cutoffSpan * static_cast<double>(bin) / static_cast<double>(resamplerCutoffBins - 1);
        for (std::size_t phase = 0; phase < resamplerPhases; ++phase) {
            const double fraction = static_cast<double>(phase) / static_cast<double>(resamplerPhases);
            const auto base = (bin * resamplerPhases + phase) * resamplerTaps;
            double sum = 0.0;
            for (std::size_t tap = 0; tap < resamplerTaps; ++tap) {
                const double x = static_cast<double>(firstTap + static_cast<int>(tap)) - fraction;
                const double distance = std::abs(x);
                double weight = 0.0;
                if (distance < radius) {
                    const double window = 0.42
                        + 0.5 * std::cos(std::numbers::pi * x / radius)
                        + 0.08 * std::cos(2.0 * std::numbers::pi * x / radius);
                    const double sincArgument = cutoff * x;
                    const double sinc = std::abs(sincArgument) < 1.0e-12
                        ? 1.0
                        : std::sin(std::numbers::pi * sincArgument)
                            / (std::numbers::pi * sincArgument);
                    weight = cutoff * sinc * window;
                }
                resamplerKernels[base + tap] = static_cast<float>(weight);
                sum += weight;
            }
            if (std::abs(sum) > 1.0e-12) {
                const float scale = static_cast<float>(1.0 / sum);
                for (std::size_t tap = 0; tap < resamplerTaps; ++tap)
                    resamplerKernels[base + tap] *= scale;
            }
        }
    }
}

const float* Engine::resamplerKernel(double step, double cursor) const noexcept {
    if (resamplerKernels.empty() || !std::isfinite(step) || step <= 1.0001 || !std::isfinite(cursor))
        return nullptr;
    constexpr double guard = 0.985;
    const double desiredCutoff = std::clamp(guard / step,
        static_cast<double>(resamplerMinCutoff), 1.0);
    const double cutoffPosition = (desiredCutoff - static_cast<double>(resamplerMinCutoff))
        / (1.0 - static_cast<double>(resamplerMinCutoff));
    const auto bin = std::min<std::size_t>(resamplerCutoffBins - 1,
        static_cast<std::size_t>(std::floor(cutoffPosition * static_cast<double>(resamplerCutoffBins - 1))));
    const double fraction = cursor - std::floor(cursor);
    const auto phase = std::min<std::size_t>(resamplerPhases - 1,
        static_cast<std::size_t>(fraction * static_cast<double>(resamplerPhases)));
    return resamplerKernels.data() + (bin * resamplerPhases + phase) * resamplerTaps;
}

void Engine::prepare(double rate, int maxAudioBlockFrames) {
    if (!std::isfinite(rate) || rate < 8000.0 || rate > 192000.0)
        throw std::invalid_argument("Output sample rate must be 8-192 kHz");
    if (maxAudioBlockFrames <= 0 || maxAudioBlockFrames > defaultMaxAudioBlockFrames)
        throw std::invalid_argument("Max audio block must be 1-8192 frames");
    sampleRate = rate;
    maxBlockFrames = maxAudioBlockFrames;
    prepareResamplerKernels();
    lowCoeff = static_cast<float>(1.0 - std::exp(-2.0 * std::numbers::pi * 200.0 / rate));
    highCoeff = static_cast<float>(1.0 - std::exp(-2.0 * std::numbers::pi * 2400.0 / rate));
    smoothing = static_cast<float>(1.0 - std::exp(-1.0 / (rate * 0.005)));
    transitionSamples = std::max(1, static_cast<int>(std::lround(rate * 0.005)));
    masterSmooth = 0.0f;
    crossSmooth = bounded(crossfader.load(std::memory_order_relaxed), 0.0f, 1.0f, 0.5f);
    headphoneSmooth = 0.5f;
    const auto initialCrossGains = crossfaderGains(
        crossSmooth,
        crossfaderCurveFromRaw(crossfaderCurve.load(std::memory_order_relaxed)));
    crossGainSmooth = {initialCrossGains.left, initialCrossGains.right};
    for (auto& scratch : sourceScratch)
        for (auto& channel : scratch)
            channel.assign(static_cast<std::size_t>(maxBlockFrames), 0.0f);
    for (auto& s : states) {
        for (auto& channel : s.delay) channel.assign(static_cast<std::size_t>(rate * 0.25), 0.0f);
        s.delayIndex = 0;
        s.bass.fill(0.0f);
        s.treble.fill(0.0f);
        s.lastProcessed.fill(0.0f);
        s.transitionFrom.fill(0.0f);
        s.cursor = 0.0;
        s.audibleCursor = 0.0;
        s.trim = 1.0f;
        s.gain = 0.0f;
        s.rate = 1.0f;
        s.cue = 0.0f;
        s.low = s.mid = s.high = 1.0f;
        s.echo = s.drive = 0.0f;
        s.transitionRemaining = 0;
        s.wasPlaying = false;
        s.wasReverse = false;
        s.wasSlip = false;
        s.streamReady = true;
    }
}
bool Engine::submit(std::size_t deck, std::unique_ptr<Clip> clip) {
    if (deck >= deckCount || !clip || !clip->valid()) return false;
    clips[deck].publish(std::move(clip));
    return true;
}
bool Engine::eject(std::size_t deck) noexcept {
    if (deck >= deckCount) return false;
    auto& control = controls[deck];
    control.playing.store(false, std::memory_order_release);
    control.reverse.store(false, std::memory_order_release);
    control.slip.store(false, std::memory_order_release);
    clearLoopRegion(deck);
    clips[deck].requestClear();
    return true;
}
void Engine::collectRetired() noexcept { for (auto& c : clips) c.collect(); }
bool Engine::setDeckSourceRenderer(std::size_t deck, DeckSourceRenderer* renderer) noexcept {
    if (deck >= deckCount) return false;
    // nullptr removes an optional/research provider and restores the built-in
    // loop-region owner instead of leaving production transport unowned.
    sourceRenderers[deck] = renderer != nullptr ? renderer : &loopRegionRenderers[deck];
    return true;
}
void Engine::process(float* const* output, int channels, int frames) noexcept {
    if (output == nullptr || channels <= 0 || frames <= 0) return;
    for (int c = 0; c < channels; ++c)
        if (output[c] != nullptr) std::fill_n(output[c], frames, 0.0f);
    struct Block {
        const Clip* clip{};
        bool playing{}, loop{}, cue{}, reverse{}, slip{}, externalRendered{};
        float trim{}, gain{}, rate{}, low{}, mid{}, high{}, echo{}, drive{};
        double externalNextCursor = 0.0;
        double externalAudibleCursor = 0.0;
    };
    std::array<Block, deckCount> blocks{};
    std::array<float, deckCount> preFaderPeaks{};
    std::array<float, deckCount> peaks{};
    std::array<double, deckCount> rmsEnergy{};
    std::array<bool, deckCount> deckOverload{};
    for (std::size_t d = 0; d < deckCount; ++d) {
        auto& state = states[d];
        auto& control = controls[d];
        if (clips[d].adopt()) {
            state.cursor = 0.0;
            state.audibleCursor = 0.0;
            state.trim = 1.0f;
            state.gain = 0.0f;
            state.rate = 1.0f;
            state.cue = 0.0f;
            state.bass.fill(0.0f);
            state.treble.fill(0.0f);
            state.lastProcessed.fill(0.0f);
            state.transitionFrom.fill(0.0f);
            state.transitionRemaining = 0;
            state.wasPlaying = false;
            state.wasReverse = false;
            state.wasSlip = false;
            for (auto& channel : state.delay) std::fill(channel.begin(), channel.end(), 0.0f);
            state.delayIndex = 0;
            control.playing.store(false, std::memory_order_relaxed);
            control.reverse.store(false, std::memory_order_relaxed);
            control.slip.store(false, std::memory_order_relaxed);
            if (const auto* adopted = clips[d].current(); adopted) {
                state.streamReady = !adopted->stream;
                if (adopted->stream) adopted->stream->request(0);
            } else {
                state.streamReady = true;
            }
        }
        auto& b = blocks[d];
        b.clip = clips[d].current();
        b.playing = control.playing.load(std::memory_order_relaxed);
        b.loop = control.loop.load(std::memory_order_relaxed);
        b.cue = control.headphone.load(std::memory_order_relaxed);
        b.reverse = control.reverse.load(std::memory_order_relaxed);
        b.slip = control.slip.load(std::memory_order_relaxed);
        b.trim = decibelsToGain(control.trimDb.load(std::memory_order_relaxed));
        b.gain = bounded(control.gain.load(), 0.0f, 1.5f);
        b.rate = bounded(control.rate.load(), 0.5f, 1.5f, 1.0f);
        b.low = bounded(control.low.load(), 0.0f, 2.0f, 1.0f);
        b.mid = bounded(control.mid.load(), 0.0f, 2.0f, 1.0f);
        b.high = bounded(control.high.load(), 0.0f, 2.0f, 1.0f);
        b.echo = bounded(control.echo.load(), 0.0f, 0.7f);
        b.drive = bounded(control.drive.load(), 0.0f, 6.0f);

        // Reverse/slip is a production built-in transport mode. Beat-region
        // looping and external/research source renderers have independent
        // transport contracts, so combinations fail closed rather than silently
        // mixing incompatible cursor ownership.
        if ((b.reverse || b.slip)
            && (loopRegionRenderers[d].regionEnabled()
                || sourceRenderers[d] != &loopRegionRenderers[d])) {
            b.reverse = false;
            b.slip = false;
            control.reverse.store(false, std::memory_order_relaxed);
            control.slip.store(false, std::memory_order_relaxed);
        }

        const auto seek = control.seek.exchange(-1.0, std::memory_order_relaxed);
        if (b.clip && std::isfinite(seek) && seek >= 0.0) {
            state.transitionFrom = state.lastProcessed;
            state.transitionRemaining = transitionSamples;
            state.cursor = std::clamp(seek, 0.0, 1.0) * static_cast<double>(b.clip->frames() - 1);
            state.audibleCursor = state.cursor;
            if (b.clip->stream)
                b.clip->stream->request(static_cast<std::int64_t>(state.audibleCursor));
        } else if (b.clip && b.clip->stream) {
            const auto requestFrame = static_cast<std::int64_t>(std::clamp(
                state.audibleCursor, 0.0,
                static_cast<double>(std::max<std::int64_t>(0, b.clip->frames() - 1))));
            b.clip->stream->request(requestFrame);
        }

        if (b.clip && b.playing && frames <= maxBlockFrames && sourceRenderers[d] != nullptr) {
            double nextTransport = state.cursor;
            double nextAudible = state.audibleCursor;
            auto& scratch = sourceScratch[d];
            const bool rendered = sourceRenderers[d]->render(
                *b.clip, state.cursor, b.loop, static_cast<double>(b.rate),
                scratch[0].data(), scratch[1].data(), frames, nextTransport, nextAudible);
            if (rendered && std::isfinite(nextTransport) && std::isfinite(nextAudible)) {
                b.externalRendered = true;
                b.externalNextCursor = nextTransport;
                b.externalAudibleCursor = nextAudible;
            }
        }

        if (!b.externalRendered && (b.reverse != state.wasReverse || b.slip != state.wasSlip)) {
            state.transitionFrom = state.lastProcessed;
            state.transitionRemaining = transitionSamples;
            if (state.wasReverse && state.wasSlip && b.reverse && !b.slip)
                state.cursor = state.audibleCursor;
            if (!b.reverse || !state.wasReverse)
                state.audibleCursor = state.cursor;
            state.wasReverse = b.reverse;
            state.wasSlip = b.slip;
        }
    }
    const float crossTarget = bounded(crossfader.load(), 0.0f, 1.0f, 0.5f);
    const auto crossCurve = crossfaderCurveFromRaw(crossfaderCurve.load(std::memory_order_relaxed));
    const float masterTarget = bounded(master.load(), 0.0f, 1.0f);
    const float headphoneTarget = bounded(headphoneLevel.load(), 0.0f, 1.0f);
    float peak = 0.0f;
    double masterRmsEnergy = 0.0;
    bool overload = false;
    for (int frame = 0; frame < frames; ++frame) {
        crossSmooth += smoothing * (crossTarget - crossSmooth);
        masterSmooth += smoothing * (masterTarget - masterSmooth);
        headphoneSmooth += smoothing * (headphoneTarget - headphoneSmooth);
        const auto crossGainTarget = crossfaderGains(crossSmooth, crossCurve);
        crossGainSmooth[0] += smoothing * (crossGainTarget.left - crossGainSmooth[0]);
        crossGainSmooth[1] += smoothing * (crossGainTarget.right - crossGainSmooth[1]);
        const float leftFade = crossGainSmooth[0];
        const float rightFade = crossGainSmooth[1];
        std::array<float, 2> mix{}, cueMix{};
        for (std::size_t d = 0; d < deckCount; ++d) {
            auto& s = states[d];
            auto& b = blocks[d];
            if (b.playing != s.wasPlaying) {
                s.transitionFrom = s.lastProcessed;
                s.transitionRemaining = transitionSamples;
                s.wasPlaying = b.playing;
            }
            s.trim += smoothing * (b.trim - s.trim);
            s.gain += smoothing * (b.gain - s.gain);
            s.rate += smoothing * (b.rate - s.rate);
            s.cue += smoothing * ((b.cue ? 1.0f : 0.0f) - s.cue);
            s.low += smoothing * (b.low - s.low);
            s.mid += smoothing * (b.mid - s.mid);
            s.high += smoothing * (b.high - s.high);
            s.echo += smoothing * (b.echo - s.echo);
            s.drive += smoothing * (b.drive - s.drive);
            std::array<float, 2> sample{};
            bool streamFrameReady = true;
            if (b.clip && b.playing) {
                if (b.externalRendered) {
                    sample[0] = sourceScratch[d][0][static_cast<std::size_t>(frame)];
                    sample[1] = sourceScratch[d][1][static_cast<std::size_t>(frame)];
                } else {
                    const auto lengthFrames = b.clip->frames();
                    const auto length = static_cast<double>(lengthFrames);

                    if (b.reverse && b.slip && s.cursor >= length) {
                        if (b.loop) {
                            s.transitionFrom = s.lastProcessed;
                            s.transitionRemaining = transitionSamples;
                            s.cursor = std::fmod(s.cursor, length);
                        } else {
                            b.playing = false;
                            controls[d].playing.store(false, std::memory_order_relaxed);
                            if (s.wasPlaying) {
                                s.transitionFrom = s.lastProcessed;
                                s.transitionRemaining = transitionSamples;
                                s.wasPlaying = false;
                            }
                        }
                    }

                    if (b.reverse && s.audibleCursor < 0.0) {
                        if (b.loop) {
                            s.transitionFrom = s.lastProcessed;
                            s.transitionRemaining = transitionSamples;
                            double wrapped = std::fmod(s.audibleCursor, length);
                            if (wrapped < 0.0) wrapped += length;
                            s.audibleCursor = wrapped;
                            if (!b.slip) s.cursor = s.audibleCursor;
                            if (b.clip->stream)
                                b.clip->stream->request(static_cast<std::int64_t>(s.audibleCursor));
                        } else if (b.slip) {
                            // Audible reverse reached track start while the slip
                            // timeline continued. Rejoin the hidden transport
                            // with the prepared transition instead of pinning at 0.
                            b.reverse = false;
                            controls[d].reverse.store(false, std::memory_order_relaxed);
                            s.wasReverse = false;
                            s.transitionFrom = s.lastProcessed;
                            s.transitionRemaining = transitionSamples;
                            s.audibleCursor = s.cursor;
                        } else {
                            b.playing = false;
                            controls[d].playing.store(false, std::memory_order_relaxed);
                            s.cursor = 0.0;
                            s.audibleCursor = 0.0;
                            if (s.wasPlaying) {
                                s.transitionFrom = s.lastProcessed;
                                s.transitionRemaining = transitionSamples;
                                s.wasPlaying = false;
                            }
                        }
                    }

                    if (!b.reverse && s.cursor >= length) {
                        if (b.loop) {
                            s.transitionFrom = s.lastProcessed;
                            s.transitionRemaining = transitionSamples;
                            s.cursor = std::fmod(s.cursor, length);
                            s.audibleCursor = s.cursor;
                            if (b.clip->stream)
                                b.clip->stream->request(static_cast<std::int64_t>(s.cursor));
                        } else {
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
                        const double step = b.clip->sampleRate / sampleRate * static_cast<double>(s.rate);
                        const double readCursor = b.reverse ? s.audibleCursor : s.cursor;
                        const auto* kernel = resamplerKernel(step, readCursor);
                        const auto left = resample(*b.clip, 0, readCursor, b.loop, kernel);
                        const auto right = resample(*b.clip, 1, readCursor, b.loop, kernel);
                        sample[0] = left.value;
                        sample[1] = right.value;
                        streamFrameReady = left.ready && right.ready;
                        if (b.clip->stream) {
                            const auto streamFrame = static_cast<std::int64_t>(std::clamp(
                                readCursor, 0.0,
                                static_cast<double>(std::max<std::int64_t>(0, lengthFrames - 1))));
                            if (!streamFrameReady) b.clip->stream->noteStarvation(streamFrame);
                            else if (!s.streamReady) b.clip->stream->noteRefill();
                            if (streamFrameReady != s.streamReady) {
                                s.transitionFrom = s.lastProcessed;
                                s.transitionRemaining = transitionSamples;
                                s.streamReady = streamFrameReady;
                            }
                        }

                        if (b.reverse) {
                            s.audibleCursor -= step;
                            if (b.slip) s.cursor += step;
                            else s.cursor = s.audibleCursor;
                        } else {
                            s.cursor += step;
                            s.audibleCursor = s.cursor;
                        }
                    }
                }
            }
            const float transitionMix = s.transitionRemaining > 0
                ? 1.0f - static_cast<float>(s.transitionRemaining) / static_cast<float>(transitionSamples)
                : 1.0f;
            for (std::size_t c = 0; c < 2; ++c) {
                // Input trim lives before EQ/FX and therefore also affects the
                // pre-fader headphone cue. The channel fader remains post-FX.
                const float in = bounded(sample[c] * s.trim, -16.0f, 16.0f);
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
                preFaderPeaks[d] = std::max(preFaderPeaks[d], std::abs(x));
                deckOverload[d] = deckOverload[d] || std::abs(x) > 1.0f;
                if (s.cue > 0.0001f) cueMix[c] += x * headphoneSmooth * s.cue;
                x *= s.gain;
                peaks[d] = std::max(peaks[d], std::abs(x));
                rmsEnergy[d] += static_cast<double>(x) * static_cast<double>(x);
                mix[c] += x * ((d % 2 == 0) ? leftFade : rightFade);
            }
            if (s.transitionRemaining > 0) --s.transitionRemaining;
            if (!s.delay[0].empty()) s.delayIndex = (s.delayIndex + 1) % s.delay[0].size();
        }
        for (int c = 0; c < 2; ++c) {
            const float x = clean(mix[static_cast<std::size_t>(c)] * masterSmooth);
            overload = overload || std::abs(x) > 0.98f;
            peak = std::max(peak, std::abs(x));
            masterRmsEnergy += static_cast<double>(x) * static_cast<double>(x);
            if (c < channels && output[c]) output[c][frame] = protectOutput(x);
            if (channels >= 4 && output[c + 2])
                output[c + 2][frame] = protectOutput(cueMix[static_cast<std::size_t>(c)]);
        }
    }
    const double stereoSampleCount = static_cast<double>(frames) * 2.0;
    for (std::size_t d = 0; d < deckCount; ++d) {
        const auto* clip = blocks[d].clip;
        const auto frameCount = clip ? clip->frames() : 0;
        if (blocks[d].externalRendered && clip) {
            states[d].cursor = blocks[d].externalNextCursor;
            states[d].audibleCursor = blocks[d].externalAudibleCursor;
            if (!blocks[d].loop && states[d].cursor >= static_cast<double>(frameCount))
                controls[d].playing.store(false, std::memory_order_relaxed);
        } else if (!blocks[d].reverse) {
            states[d].audibleCursor = states[d].cursor;
        }
        meters[d].duration.store(clip ? static_cast<double>(frameCount) / clip->sampleRate : 0.0);
        meters[d].position.store(clip
            ? std::clamp(states[d].cursor, 0.0, static_cast<double>(frameCount)) / clip->sampleRate
            : 0.0);
        meters[d].audiblePosition.store(clip
            ? std::clamp(states[d].audibleCursor, 0.0, static_cast<double>(frameCount)) / clip->sampleRate
            : 0.0);
        meters[d].preFaderPeak.store(preFaderPeaks[d], std::memory_order_relaxed);
        meters[d].peak.store(peaks[d], std::memory_order_relaxed);
        meters[d].rms.store(stereoSampleCount > 0.0
            ? static_cast<float>(std::sqrt(rmsEnergy[d] / stereoSampleCount)) : 0.0f,
            std::memory_order_relaxed);
        meters[d].overloaded.store(deckOverload[d], std::memory_order_relaxed);
        if (clip && clip->stream) {
            const auto requestedCursor = blocks[d].reverse
                ? states[d].audibleCursor : states[d].cursor;
            clip->stream->request(static_cast<std::int64_t>(std::clamp(
                requestedCursor, 0.0,
                static_cast<double>(std::max<std::int64_t>(0, frameCount - 1)))));
        }
    }
    masterPeak.store(peak, std::memory_order_relaxed);
    masterRms.store(stereoSampleCount > 0.0
        ? static_cast<float>(std::sqrt(masterRmsEnergy / stereoSampleCount)) : 0.0f,
        std::memory_order_relaxed);
    clipped.store(overload, std::memory_order_relaxed);
}
} // namespace broke
