// SPDX-License-Identifier: AGPL-3.0-only
#include "core/TimeStretchDeck.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace {
std::atomic<bool> trackHeap{false};
std::atomic<std::size_t> allocations{0};
std::atomic<std::size_t> deallocations{0};
int checks = 0;

void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

void* allocate(std::size_t size) {
    if (trackHeap.load(std::memory_order_relaxed)) allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* pointer = std::malloc(size == 0 ? 1 : size)) return pointer;
    throw std::bad_alloc();
}

void release(void* pointer) noexcept {
    if (pointer != nullptr && trackHeap.load(std::memory_order_relaxed))
        deallocations.fetch_add(1, std::memory_order_relaxed);
    std::free(pointer);
}

void fillTone(float* left, float* right, int frames, double& phase) {
    constexpr double twoPi = 6.28318530717958647692;
    constexpr double advance = twoPi * 440.0 / 48000.0;
    for (int i = 0; i < frames; ++i) {
        const float value = static_cast<float>(0.18 * std::sin(phase));
        left[i] = value;
        right[i] = value * 0.8f;
        phase += advance;
        if (phase >= twoPi) phase -= twoPi;
    }
}

void processBlock(broke::TimeStretchDeckAdapter& deck,
                  std::array<float, 2048>& left,
                  std::array<float, 2048>& right,
                  std::array<float, 1024>& outLeft,
                  std::array<float, 1024>& outRight,
                  double& phase) {
    const int needed = deck.inputFramesForOutput(static_cast<int>(outLeft.size()));
    if (needed <= 0 || needed > static_cast<int>(left.size()))
        throw std::runtime_error("deck clock produced invalid bounded input request");
    fillTone(left.data(), right.data(), needed, phase);
    if (!deck.processStereo(left.data(), right.data(), needed,
                            outLeft.data(), outRight.data(), static_cast<int>(outLeft.size())))
        throw std::runtime_error("deck clock processing failed");
}

void run() {
    broke::TimeStretchDeckAdapter deck;
    check(deck.prepare(48000.0, 1024, 2.0), "deck clock prepares for bounded realtime blocks");
    check(deck.setPitchSemitones(0.0f), "neutral key-lock pitch accepted");
    check(deck.setPlaybackRate(1.25), "initial playback rate accepted");

    std::array<float, 2048> left{};
    std::array<float, 2048> right{};
    std::array<float, 1024> outLeft{};
    std::array<float, 1024> outRight{};
    double phase = 0.0;

    for (int block = 0; block < 80; ++block)
        processBlock(deck, left, right, outLeft, outRight, phase);

    allocations.store(0, std::memory_order_relaxed);
    deallocations.store(0, std::memory_order_relaxed);
    const auto sourceBefore = deck.sourceFramesConsumed();
    constexpr int blocks = 600;
    const auto started = std::chrono::steady_clock::now();
    trackHeap.store(true, std::memory_order_seq_cst);
    for (int block = 0; block < blocks; ++block) {
        if ((block % 100) == 0) {
            const int phaseIndex = (block / 100) % 3;
            const double rate = phaseIndex == 0 ? 1.0 : (phaseIndex == 1 ? 1.25 : 1.5);
            if (!deck.setPlaybackRate(rate)) {
                trackHeap.store(false, std::memory_order_seq_cst);
                throw std::runtime_error("bounded playback-rate automation accepted");
            }
        }
        processBlock(deck, left, right, outLeft, outRight, phase);
    }
    trackHeap.store(false, std::memory_order_seq_cst);
    const auto finished = std::chrono::steady_clock::now();

    const auto observedAllocations = allocations.load(std::memory_order_relaxed);
    const auto observedDeallocations = deallocations.load(std::memory_order_relaxed);
    check(observedAllocations == 0, "deck clock performs no heap allocation after prepare/warmup");
    check(observedDeallocations == 0, "deck clock performs no heap deallocation after prepare/warmup");
    check(deck.sourceFramesConsumed() > sourceBefore, "deck source clock advances during measured window");
    check(deck.fractionalSourceCarry() >= 0.0 && deck.fractionalSourceCarry() < 1.0,
          "deck fractional source carry remains normalized");
    check(std::all_of(outLeft.begin(), outLeft.end(), [](float value) { return std::isfinite(value); }),
          "left deck output remains finite");
    check(std::all_of(outRight.begin(), outRight.end(), [](float value) { return std::isfinite(value); }),
          "right deck output remains finite");

    const auto elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started).count();
    constexpr double outputFrames = static_cast<double>(blocks) * 1024.0;
    const double nsPerOutputFrame = static_cast<double>(elapsedNs) / outputFrames;
    check(elapsedNs > 0 && std::isfinite(nsPerOutputFrame) && nsPerOutputFrame > 0.0,
          "deck clock diagnostic cost is finite and positive");

    std::cout << "METRIC deck_timestretch_blocks=" << blocks
              << " ns_per_output_frame=" << nsPerOutputFrame
              << " source_frames_consumed=" << (deck.sourceFramesConsumed() - sourceBefore)
              << " heap_allocations=" << observedAllocations
              << " heap_deallocations=" << observedDeallocations
              << " timing_is_diagnostic_only=1\n";
}
} // namespace

void* operator new(std::size_t size) { return allocate(size); }
void* operator new[](std::size_t size) { return allocate(size); }
void operator delete(void* pointer) noexcept { release(pointer); }
void operator delete[](void* pointer) noexcept { release(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { release(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { release(pointer); }

int main() {
    try {
        run();
        std::cout << "PASS: " << checks << " deck time-stretch realtime checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        trackHeap.store(false, std::memory_order_relaxed);
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
