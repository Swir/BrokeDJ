// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace broke {
inline constexpr std::size_t deckCount = 4;

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
    [[nodiscard]] std::int64_t requestedFrame() const noexcept {
        return requested.load(std::memory_order_relaxed);
    }
    // Non-audio diagnostic snapshot. It reports whether the exact transport
    // request is resident plus bounded forward cache coverage and lock-free
    // read-starvation history. These counters describe cache misses/recovery,
    // not physical audio-device underruns.
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
    void noteMiss(std::int64_t frame) const noexcept;
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

// UI -> audio controls. Seek is a last-request-wins normalized mailbox.
struct Controls final {
    std::atomic<bool> playing{false}, loop{false}, headphone{false};
    std::atomic<float> gain{0.7f}, rate{1.0f};
    std::atomic<float> low{1.0f}, mid{1.0f}, high{1.0f};
    std::atomic<float> echo{0.0f}, drive{0.0f};
    std::atomic<double> seek{-1.0};
};
struct Meter final {
    std::atomic<double> position{0.0}, duration{0.0};
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
public:
    Engine() = default;
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    // Call only while audio is stopped. Allocates the fixed-delay buffers.
    void prepare(double outputSampleRate);
    [[nodiscard]] bool submit(std::size_t deck, std::unique_ptr<Clip> clip);
    void collectRetired() noexcept;
    // Layout: master L/R, optional headphone L/R. Other channels are cleared.
    // The output pointers may be null. No resizing/allocation in this method.
    void process(float* const* output, int channels, int frames) noexcept;
    Controls& control(std::size_t deck) { return controls.at(deck); }
    const Meter& meter(std::size_t deck) const { return meters.at(deck); }
    std::atomic<float> crossfader{0.5f}, master{0.5f}, headphoneLevel{0.5f};
    std::atomic<float> masterPeak{0.0f};
    std::atomic<bool> clipped{false};
private:
    struct State {
        double cursor = 0.0;
        float gain = 0.0f, rate = 1.0f, cue = 0.0f;
        float low = 1.0f, mid = 1.0f, high = 1.0f;
        float echo = 0.0f, drive = 0.0f;
        std::array<float, 2> bass{}, treble{};
        std::array<std::vector<float>, 2> delay;
        std::size_t delayIndex = 0;
        std::array<float, 2> lastProcessed{}, transitionFrom{};
        int transitionRemaining = 0;
        bool wasPlaying = false;
        bool streamReady = true;
    };
    std::array<Controls, deckCount> controls;
    std::array<Meter, deckCount> meters;
    std::array<ClipMailbox, deckCount> clips;
    std::array<State, deckCount> states;
    double sampleRate = 44100.0;
    float lowCoeff = 0.0f, highCoeff = 0.0f, smoothing = 0.0f;
    float masterSmooth = 0.0f, crossSmooth = 0.5f, headphoneSmooth = 0.5f;
    int transitionSamples = 1;
};
} // namespace broke
