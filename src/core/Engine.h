// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <vector>

namespace broke {
inline constexpr std::size_t deckCount = 4;
inline constexpr std::size_t resamplerTaps = 24;
inline constexpr std::size_t resamplerPhases = 128;
inline constexpr std::size_t resamplerCutoffBins = 64;
inline constexpr float resamplerMinCutoff = 0.06f;
inline constexpr int defaultMaxAudioBlockFrames = 8192;

struct StreamCacheDiagnostics final {
    std::int64_t requestedFrame = 0;
    std::int64_t requestedChunk = 0;
    std::int64_t lastMissFrame = -1;
    std::size_t readyChunks = 0;
    std::size_t inspectedChunks = 0;
    std::uint64_t readMisses = 0;
    std::uint64_t starvationEvents = 0;
    std::uint64_t refillEvents = 0;
    bool requestedRegionReady = false;
    bool starving = false;
};

// Bounded, lock-free read-ahead cache shared by a background decoder and the
// audio callback. Cache writes may race with reads safely because sample cells
// are atomic and every slot is published only after a complete chunk write.
class StreamCache final {
public:
    static constexpr std::size_t chunkFrames = 4096;
    static constexpr std::size_t slotCount = 32;

    explicit StreamCache(std::int64_t totalFrames);
    StreamCache(const StreamCache&) = delete;
    StreamCache& operator=(const StreamCache&) = delete;

    [[nodiscard]] std::int64_t totalFrames() const noexcept { return total; }
    [[nodiscard]] bool hasChunk(std::int64_t chunkIndex) const noexcept;
    void publishChunk(std::int64_t chunkIndex, const float* left, const float* right,
                      std::size_t frames) noexcept;
    [[nodiscard]] bool trySample(int channel, std::int64_t frame, float& value) const noexcept;
    [[nodiscard]] float sample(int channel, std::int64_t frame) const noexcept;
    void request(std::int64_t frame) const noexcept;
    // Called from the audio callback only after a complete interpolation read
    // has been classified. This avoids counting every interpolation tap as a
    // separate starvation event while remaining lock-free.
    void noteStarvation(std::int64_t frame) const noexcept;
    void noteRefill() const noexcept;
    [[nodiscard]] std::int64_t requestedFrame() const noexcept {
        return requested.load(std::memory_order_relaxed);
    }
    // Non-audio diagnostic snapshot. It reports whether the exact transport
    // request is resident plus bounded forward cache coverage and lock-free
    // read/starvation history. These counters describe cache behavior, not
    // physical audio-device underruns.
    [[nodiscard]] StreamCacheDiagnostics diagnostics(std::size_t lookAheadChunks = 12) const noexcept {
        StreamCacheDiagnostics result;
        result.readMisses = readMisses.load(std::memory_order_relaxed);
        result.starvationEvents = starvationEvents.load(std::memory_order_relaxed);
        result.refillEvents = refillEvents.load(std::memory_order_relaxed);
        result.lastMissFrame = lastMissFrame.load(std::memory_order_relaxed);
        result.starving = starving.load(std::memory_order_relaxed);
        if (total <= 0) return result;
        result.requestedFrame = requestedFrame();
        result.requestedChunk = result.requestedFrame / static_cast<std::int64_t>(chunkFrames);
        const auto totalChunks = (total + static_cast<std::int64_t>(chunkFrames) - 1)
            / static_cast<std::int64_t>(chunkFrames);
        const auto available = std::max<std::int64_t>(0, totalChunks - result.requestedChunk);
        result.inspectedChunks = static_cast<std::size_t>(std::min<std::int64_t>(
            std::max<std::int64_t>(1, static_cast<std::int64_t>(lookAheadChunks)), available));
        result.requestedRegionReady = hasChunk(result.requestedChunk);
        for (std::size_t i = 0; i < result.inspectedChunks; ++i) {
            if (hasChunk(result.requestedChunk + static_cast<std::int64_t>(i))) ++result.readyChunks;
        }
        return result;
    }

private:
    struct Slot final {
        Slot();
        std::atomic<std::int64_t> chunk{-1};
        std::atomic<std::size_t> validFrames{0};
        std::unique_ptr<std::atomic<float>[]> left;
        std::unique_ptr<std::atomic<float>[]> right;
    };
    [[nodiscard]] static std::size_t slotFor(std::int64_t chunkIndex) noexcept;
    void noteReadMiss(std::int64_t frame) const noexcept;
    std::array<Slot, slotCount> slots;
    std::int64_t total = 0;
    mutable std::atomic<std::int64_t> requested{0};
    mutable std::atomic<std::int64_t> lastMissFrame{-1};
    mutable std::atomic<std::uint64_t> readMisses{0};
    mutable std::atomic<std::uint64_t> starvationEvents{0};
    mutable std::atomic<std::uint64_t> refillEvents{0};
    mutable std::atomic<bool> starving{false};
};

struct Clip final {
    double sampleRate = 44100.0;
    std::vector<float> left, right;
    std::shared_ptr<StreamCache> stream;
    std::shared_ptr<void> sourceOwner; // keeps a non-core background source alive; never touched by audio code
    std::int64_t frameCount = 0;
    [[nodiscard]] std::int64_t frames() const noexcept;
    [[nodiscard]] bool streamed() const noexcept { return static_cast<bool>(stream); }
    [[nodiscard]] bool valid() const noexcept;
};

// Optional raw deck-source boundary. Implementations may supply one complete
// source block (for example, a qualified key-lock path) before Engine applies
// its existing per-deck EQ/FX/gain/cue/meter and master routing. The pointer is
// non-owning and must be installed/removed only while audio is stopped. render()
// is called on the realtime thread and therefore must be bounded, allocation-
// free, non-blocking and free of I/O/decoding/logging. Returning false asks
// Engine to use its built-in production rate converter immediately.
class DeckSourceRenderer {
public:
    virtual ~DeckSourceRenderer() = default;
    [[nodiscard]] virtual bool render(const Clip& clip, double cursor, bool loop,
                                      double playbackRate,
                                      float* outputLeft, float* outputRight,
                                      int deviceFrames, double& nextTransportCursor,
                                      double& nextAudibleCursor) noexcept = 0;
};

// UI -> audio controls. Seek is a last-request-wins normalized mailbox.
// Reverse changes the audible/source direction. Slip keeps the underlying
// transport moving forward while reverse is audible, so releasing reverse can
// rejoin the uninterrupted timeline. Beat-loop and external source renderers
// reject these modes fail-closed in Engine::process().
struct Controls final {
    std::atomic<bool> playing{false}, loop{false}, headphone{false};
    std::atomic<bool> reverse{false}, slip{false};
    std::atomic<float> gain{0.7f}, rate{1.0f};
    std::atomic<float> low{1.0f}, mid{1.0f}, high{1.0f};
    std::atomic<float> echo{0.0f}, drive{0.0f};
    std::atomic<double> seek{-1.0};
};
struct Meter final {
    std::atomic<double> position{0.0}, audiblePosition{0.0}, duration{0.0};
    std::atomic<float> peak{0.0f};
};

// Immutable clip ownership: publisher -> pending -> audio -> retired -> publisher.
// Publishing and garbage collection must happen on ONE non-audio thread.
// The audio callback never deletes a clip, allocates, or takes a mutex.
class ClipMailbox final {
public:
    ~ClipMailbox(); // Audio callback MUST have stopped before destruction.
    ClipMailbox() = default;
    ClipMailbox(const ClipMailbox&) = delete;
    ClipMailbox& operator=(const ClipMailbox&) = delete;
    void publish(std::unique_ptr<Clip> clip) noexcept;
    void collect() noexcept;
    [[nodiscard]] bool adopt() noexcept; // audio thread only
    [[nodiscard]] const Clip* current() const noexcept { return active; }
private:
    std::atomic<Clip*> pending{nullptr}, retired{nullptr};
    Clip* active = nullptr; // audio-thread-owned
};

class Engine final {
private:
    // Production beat-loop source installed from Engine construction. It is
    // dormant until an owner publishes a valid loop region. Configuration uses
    // a bounded atomic snapshot so a UI/control thread can arm/disarm a loop
    // without replacing renderer pointers while the callback is running.
    class LoopRegionRenderer final : public DeckSourceRenderer {
    public:
        LoopRegionRenderer() noexcept : kernels(&sharedKernelTable()) {}

        void bindOutputRate(const double* rate) noexcept { outputRate = rate; }

        [[nodiscard]] bool setRegion(double start, double end) noexcept {
            if (!std::isfinite(start) || !std::isfinite(end) || start < 0.0 || end <= start)
                return false;
            revision.fetch_add(1, std::memory_order_acq_rel); // odd: write in progress
            startSeconds.store(start, std::memory_order_relaxed);
            endSeconds.store(end, std::memory_order_relaxed);
            enabled.store(true, std::memory_order_release);
            revision.fetch_add(1, std::memory_order_release); // even: published
            return true;
        }

        void clearRegion() noexcept {
            revision.fetch_add(1, std::memory_order_acq_rel);
            enabled.store(false, std::memory_order_release);
            revision.fetch_add(1, std::memory_order_release);
        }

        [[nodiscard]] bool regionEnabled() const noexcept {
            return enabled.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool render(const Clip& clip, double cursor, bool loop,
                                  double playbackRate, float* outputLeft, float* outputRight,
                                  int deviceFrames, double& nextTransportCursor,
                                  double& nextAudibleCursor) noexcept override {
            if (!loop || outputLeft == nullptr || outputRight == nullptr || deviceFrames <= 0
                || !clip.valid() || !std::isfinite(cursor) || cursor < 0.0
                || !std::isfinite(playbackRate) || playbackRate < 0.5 || playbackRate > 1.5
                || outputRate == nullptr || !std::isfinite(*outputRate)
                || *outputRate < 8000.0 || *outputRate > 192000.0) {
                return false;
            }

            Snapshot snapshot;
            if (!readSnapshot(snapshot) || !snapshot.enabled) return false;
            const double duration = static_cast<double>(clip.frames()) / clip.sampleRate;
            if (!std::isfinite(duration) || snapshot.startSeconds >= duration
                || snapshot.endSeconds > duration + 1.0e-9) {
                return false;
            }

            const double startFrame = snapshot.startSeconds * clip.sampleRate;
            const double endFrame = snapshot.endSeconds * clip.sampleRate;
            const double spanFrames = endFrame - startFrame;
            if (!std::isfinite(startFrame) || !std::isfinite(endFrame)
                || startFrame < 0.0 || spanFrames <= 2.0) {
                return false;
            }

            const auto firstFrame = static_cast<std::int64_t>(std::floor(startFrame));
            const auto endExclusive = std::min<std::int64_t>(clip.frames(),
                static_cast<std::int64_t>(std::ceil(endFrame)));
            if (firstFrame < 0 || endExclusive <= firstFrame + 1) return false;

            if (snapshot.revision != activeRevision) {
                activeRevision = snapshot.revision;
                boundClip = &clip;
                lastRendered.fill(0.0f);
                transitionFrom.fill(0.0f);
                transitionRemaining = 0;
                streamReady = !clip.stream;
                smoothedRate = playbackRate;
            } else if (boundClip != &clip) {
                // A loop plan is bound to the immutable clip observed when the
                // snapshot first renders. Replacement clips fail closed until
                // the owner publishes a new plan.
                return false;
            }

            double localCursor = cursor;
            auto localLast = lastRendered;
            auto localTransitionFrom = transitionFrom;
            int localTransitionRemaining = transitionRemaining;
            double localRate = smoothedRate;
            const int transitionSamples = std::max(1,
                static_cast<int>(std::lround(*outputRate * 0.005)));
            const double rateSmoothing = 1.0 - std::exp(-1.0 / (*outputRate * 0.005));

            for (int frame = 0; frame < deviceFrames; ++frame) {
                bool wrapped = false;
                if (localCursor >= endFrame) {
                    const double overshoot = std::max(0.0, localCursor - startFrame);
                    localCursor = startFrame + std::fmod(overshoot, spanFrames);
                    localTransitionFrom = localLast;
                    localTransitionRemaining = transitionSamples;
                    wrapped = true;
                }

                localRate += rateSmoothing * (playbackRate - localRate);
                const double step = clip.sampleRate / *outputRate * localRate;
                if (!std::isfinite(step) || step <= 0.0) return false;

                const bool wrapReads = localCursor >= startFrame;
                const float* kernel = resamplerKernel(step, localCursor);
                std::array<float, 2> sample{};
                bool ready = true;
                for (int channel = 0; channel < 2; ++channel) {
                    const auto point = resample(clip, channel, localCursor, wrapReads,
                                                firstFrame, endExclusive, kernel);
                    sample[static_cast<std::size_t>(channel)] = point.value;
                    ready = ready && point.ready;
                }
                if (!ready) {
                    if (clip.stream) {
                        const auto miss = static_cast<std::int64_t>(std::clamp(
                            localCursor, 0.0,
                            static_cast<double>(std::max<std::int64_t>(0, clip.frames() - 1))));
                        clip.stream->noteStarvation(miss);
                        streamReady = false;
                    }
                    return false;
                }

                const float transitionMix = localTransitionRemaining > 0
                    ? 1.0f - static_cast<float>(localTransitionRemaining)
                        / static_cast<float>(transitionSamples)
                    : 1.0f;
                for (std::size_t channel = 0; channel < 2; ++channel) {
                    float value = std::isfinite(sample[channel]) ? sample[channel] : 0.0f;
                    if (localTransitionRemaining > 0)
                        value = localTransitionFrom[channel] * (1.0f - transitionMix)
                            + value * transitionMix;
                    localLast[channel] = value;
                    if (channel == 0) outputLeft[frame] = value;
                    else outputRight[frame] = value;
                }
                if (localTransitionRemaining > 0) --localTransitionRemaining;
                localCursor += step;
                (void)wrapped;
            }

            if (clip.stream && !streamReady) clip.stream->noteRefill();
            streamReady = true;
            lastRendered = localLast;
            transitionFrom = localTransitionFrom;
            transitionRemaining = localTransitionRemaining;
            smoothedRate = localRate;
            nextTransportCursor = localCursor;
            nextAudibleCursor = localCursor;
            return true;
        }

    private:
        struct Snapshot final {
            std::uint64_t revision = 0;
            bool enabled = false;
            double startSeconds = 0.0;
            double endSeconds = 0.0;
        };
        struct Sample final { float value = 0.0f; bool ready = false; };

        [[nodiscard]] bool readSnapshot(Snapshot& out) const noexcept {
            for (int attempt = 0; attempt < 3; ++attempt) {
                const auto before = revision.load(std::memory_order_acquire);
                if ((before & 1U) != 0U) continue;
                Snapshot candidate;
                candidate.revision = before;
                candidate.enabled = enabled.load(std::memory_order_acquire);
                candidate.startSeconds = startSeconds.load(std::memory_order_relaxed);
                candidate.endSeconds = endSeconds.load(std::memory_order_relaxed);
                const auto after = revision.load(std::memory_order_acquire);
                if (before == after && (after & 1U) == 0U) {
                    out = candidate;
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] static std::int64_t wrappedIndex(std::int64_t index,
                                                       std::int64_t first,
                                                       std::int64_t endExclusive) noexcept {
            const auto span = endExclusive - first;
            if (span <= 0) return first;
            auto offset = (index - first) % span;
            if (offset < 0) offset += span;
            return first + offset;
        }

        [[nodiscard]] static Sample sampleAt(const Clip& clip, int channel,
                                             std::int64_t index, bool wrap,
                                             std::int64_t first,
                                             std::int64_t endExclusive) noexcept {
            if (clip.frames() <= 0) return {};
            if (wrap) index = wrappedIndex(index, first, endExclusive);
            else index = std::clamp<std::int64_t>(index, 0, clip.frames() - 1);
            if (clip.stream) {
                float value = 0.0f;
                const bool ready = clip.stream->trySample(channel, index, value);
                return {std::isfinite(value) ? value : 0.0f, ready};
            }
            const auto& data = channel == 0 ? clip.left : clip.right;
            const float value = data[static_cast<std::size_t>(index)];
            return {std::isfinite(value) ? value : 0.0f, true};
        }

        [[nodiscard]] static Sample resample(const Clip& clip, int channel, double cursor,
                                             bool wrap, std::int64_t first,
                                             std::int64_t endExclusive,
                                             const float* bandlimitedKernel) noexcept {
            if (!std::isfinite(cursor)) return {};
            const auto windowFrames = endExclusive - first;
            if (bandlimitedKernel != nullptr && windowFrames >= static_cast<std::int64_t>(resamplerTaps)) {
                constexpr int firstTap = 1 - static_cast<int>(resamplerTaps / 2);
                const auto centre = static_cast<std::int64_t>(std::floor(cursor));
                float value = 0.0f;
                for (std::size_t tap = 0; tap < resamplerTaps; ++tap) {
                    const auto point = sampleAt(clip, channel,
                        centre + static_cast<std::int64_t>(firstTap + static_cast<int>(tap)),
                        wrap, first, endExclusive);
                    if (!point.ready) return {};
                    value += point.value * bandlimitedKernel[tap];
                }
                return {std::isfinite(value) ? value : 0.0f, true};
            }
            if (clip.frames() < 4) {
                const auto index = static_cast<std::int64_t>(std::floor(cursor));
                const auto fraction = static_cast<float>(cursor - static_cast<double>(index));
                const auto a = sampleAt(clip, channel, index, wrap, first, endExclusive);
                const auto b = sampleAt(clip, channel, index + 1, wrap, first, endExclusive);
                if (!a.ready || !b.ready) return {};
                return {a.value + (b.value - a.value) * fraction, true};
            }
            const auto index = static_cast<std::int64_t>(std::floor(cursor));
            const float t = static_cast<float>(cursor - static_cast<double>(index));
            const auto p0 = sampleAt(clip, channel, index - 1, wrap, first, endExclusive);
            const auto p1 = sampleAt(clip, channel, index, wrap, first, endExclusive);
            const auto p2 = sampleAt(clip, channel, index + 1, wrap, first, endExclusive);
            const auto p3 = sampleAt(clip, channel, index + 2, wrap, first, endExclusive);
            if (!p0.ready || !p1.ready || !p2.ready || !p3.ready) return {};
            const float t2 = t * t;
            const float t3 = t2 * t;
            const float value = 0.5f * ((2.0f * p1.value) + (-p0.value + p2.value) * t
                + (2.0f * p0.value - 5.0f * p1.value + 4.0f * p2.value - p3.value) * t2
                + (-p0.value + 3.0f * p1.value - 3.0f * p2.value + p3.value) * t3);
            return {std::isfinite(value) ? value : 0.0f, true};
        }

        [[nodiscard]] const float* resamplerKernel(double step, double cursor) const noexcept {
            if (kernels == nullptr || kernels->empty() || !std::isfinite(step)
                || step <= 1.0001 || !std::isfinite(cursor)) return nullptr;
            constexpr double guard = 0.985;
            const double desiredCutoff = std::clamp(guard / step,
                static_cast<double>(resamplerMinCutoff), 1.0);
            const double cutoffPosition = (desiredCutoff - static_cast<double>(resamplerMinCutoff))
                / (1.0 - static_cast<double>(resamplerMinCutoff));
            const auto bin = std::min<std::size_t>(resamplerCutoffBins - 1,
                static_cast<std::size_t>(std::floor(
                    cutoffPosition * static_cast<double>(resamplerCutoffBins - 1))));
            const double fraction = cursor - std::floor(cursor);
            const auto phase = std::min<std::size_t>(resamplerPhases - 1,
                static_cast<std::size_t>(fraction * static_cast<double>(resamplerPhases)));
            return kernels->data() + (bin * resamplerPhases + phase) * resamplerTaps;
        }

        [[nodiscard]] static const std::vector<float>& sharedKernelTable() {
            static const std::vector<float> table = [] {
                std::vector<float> values(resamplerCutoffBins * resamplerPhases * resamplerTaps, 0.0f);
                constexpr int firstTap = 1 - static_cast<int>(resamplerTaps / 2);
                const double radius = static_cast<double>(resamplerTaps) * 0.5;
                const double cutoffSpan = 1.0 - static_cast<double>(resamplerMinCutoff);
                for (std::size_t bin = 0; bin < resamplerCutoffBins; ++bin) {
                    const double cutoff = static_cast<double>(resamplerMinCutoff)
                        + cutoffSpan * static_cast<double>(bin)
                            / static_cast<double>(resamplerCutoffBins - 1);
                    for (std::size_t phase = 0; phase < resamplerPhases; ++phase) {
                        const double fraction = static_cast<double>(phase)
                            / static_cast<double>(resamplerPhases);
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
                            values[base + tap] = static_cast<float>(weight);
                            sum += weight;
                        }
                        if (std::abs(sum) > 1.0e-12) {
                            const float scale = static_cast<float>(1.0 / sum);
                            for (std::size_t tap = 0; tap < resamplerTaps; ++tap)
                                values[base + tap] *= scale;
                        }
                    }
                }
                return values;
            }();
            return table;
        }

        const std::vector<float>* kernels = nullptr;
        const double* outputRate = nullptr;
        std::atomic<std::uint64_t> revision{0};
        std::atomic<bool> enabled{false};
        std::atomic<double> startSeconds{0.0};
        std::atomic<double> endSeconds{0.0};
        std::uint64_t activeRevision = 0;
        const Clip* boundClip = nullptr;
        std::array<float, 2> lastRendered{};
        std::array<float, 2> transitionFrom{};
        int transitionRemaining = 0;
        bool streamReady = true;
        double smoothedRate = 1.0;
    };

public:
    Engine() {
        for (std::size_t deck = 0; deck < deckCount; ++deck) {
            loopRegionRenderers[deck].bindOutputRate(&sampleRate);
            sourceRenderers[deck] = &loopRegionRenderers[deck];
        }
    }
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    // Call only while audio is stopped. Allocates fixed-delay buffers, external
    // deck-source scratch blocks and the immutable windowed-sinc lookup table
    // used by the callback.
    void prepare(double outputSampleRate, int maxAudioBlockFrames = defaultMaxAudioBlockFrames);
    [[nodiscard]] bool submit(std::size_t deck, std::unique_ptr<Clip> clip);
    void collectRetired() noexcept;
    // Non-owning provider lifecycle. Call only while audio is stopped and keep
    // renderer alive until it is cleared after audio has stopped again.
    [[nodiscard]] bool setDeckSourceRenderer(std::size_t deck, DeckSourceRenderer* renderer) noexcept;
    // Atomically publish/clear a source-time loop region while audio is running.
    // The built-in loop renderer is installed from Engine construction, so no
    // renderer pointer is swapped in the callback lifecycle. If reverse/slip is
    // armed, or a research/other external renderer owns the deck, arming fails
    // closed instead of silently combining incompatible transport semantics.
    [[nodiscard]] bool setLoopRegionSeconds(std::size_t deck, double startSeconds,
                                            double endSeconds) noexcept {
        if (deck >= deckCount || sourceRenderers[deck] != &loopRegionRenderers[deck]
            || controls[deck].reverse.load(std::memory_order_relaxed)
            || controls[deck].slip.load(std::memory_order_relaxed)) {
            return false;
        }
        return loopRegionRenderers[deck].setRegion(startSeconds, endSeconds);
    }
    void clearLoopRegion(std::size_t deck) noexcept {
        if (deck < deckCount) loopRegionRenderers[deck].clearRegion();
    }
    [[nodiscard]] bool loopRegionEnabled(std::size_t deck) const noexcept {
        return deck < deckCount && loopRegionRenderers[deck].regionEnabled();
    }
    // Layout: master L/R, optional headphone L/R. Other channels are cleared.
    // The output pointers may be null. No resizing/allocation in this method.
    void process(float* const* output, int channels, int frames) noexcept;
    Controls& control(std::size_t deck) { return controls.at(deck); }
    const Meter& meter(std::size_t deck) const { return meters.at(deck); }
    [[nodiscard]] int preparedMaxAudioBlockFrames() const noexcept { return maxBlockFrames; }
    std::atomic<float> crossfader{0.5f}, master{0.5f}, headphoneLevel{0.5f};
    std::atomic<float> masterPeak{0.0f};
    std::atomic<bool> clipped{false};
private:
    struct State {
        double cursor = 0.0;
        double audibleCursor = 0.0;
        float gain = 0.0f, rate = 1.0f, cue = 0.0f;
        float low = 1.0f, mid = 1.0f, high = 1.0f;
        float echo = 0.0f, drive = 0.0f;
        std::array<float, 2> bass{}, treble{};
        std::array<std::vector<float>, 2> delay;
        std::size_t delayIndex = 0;
        std::array<float, 2> lastProcessed{}, transitionFrom{};
        int transitionRemaining = 0;
        bool wasPlaying = false;
        bool wasReverse = false;
        bool wasSlip = false;
        bool streamReady = true;
    };
    void prepareResamplerKernels();
    [[nodiscard]] const float* resamplerKernel(double step, double cursor) const noexcept;

    std::array<Controls, deckCount> controls;
    std::array<Meter, deckCount> meters;
    std::array<ClipMailbox, deckCount> clips;
    std::array<State, deckCount> states;
    std::array<LoopRegionRenderer, deckCount> loopRegionRenderers;
    std::array<DeckSourceRenderer*, deckCount> sourceRenderers{};
    std::array<std::array<std::vector<float>, 2>, deckCount> sourceScratch;
    std::vector<float> resamplerKernels;
    double sampleRate = 44100.0;
    float lowCoeff = 0.0f, highCoeff = 0.0f, smoothing = 0.0f;
    float masterSmooth = 0.0f, crossSmooth = 0.5f, headphoneSmooth = 0.5f;
    int transitionSamples = 1;
    int maxBlockFrames = 0;
};
} // namespace broke
