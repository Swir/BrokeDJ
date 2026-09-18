// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <vector>

namespace broke {
inline constexpr std::size_t deckCount = 4;
struct Clip final {
    double sampleRate = 44100.0;
    std::vector<float> left, right;
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
