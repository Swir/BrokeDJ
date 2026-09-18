// SPDX-License-Identifier: AGPL-3.0-only
#include "core/TimeStretchSourceBridge.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <numbers>
#include <stdexcept>
#include <vector>

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
    if (trackHeap.load(std::memory_order_relaxed))
        allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* pointer = std::malloc(size == 0 ? 1 : size)) return pointer;
    throw std::bad_alloc();
}

void release(void* pointer) noexcept {
    if (pointer != nullptr && trackHeap.load(std::memory_order_relaxed))
        deallocations.fetch_add(1, std::memory_order_relaxed);
    std::free(pointer);
}

broke::Clip makeToneClip() {
    constexpr double sampleRate = 48000.0;
    constexpr int seconds = 40;
    broke::Clip clip;
    clip.sampleRate = sampleRate;
    clip.left.resize(static_cast<std::size_t>(sampleRate * seconds));
    clip.right.resize(clip.left.size());
    for (std::size_t i = 0; i < clip.left.size(); ++i) {
        const double phase = 2.0 * std::numbers::pi * 440.0
            * static_cast<double>(i) / sampleRate;
        clip.left[i] = static_cast<float>(0.15 * std::sin(phase));
        clip.right[i] = clip.left[i] * 0.8f;
    }
    return clip;
}

void run() {
    constexpr int blockFrames = 1024;
    constexpr int blocks = 600;

    auto clip = makeToneClip();
    broke::TimeStretchSourceBridge bridge;
    check(bridge.prepare(48000.0, 4096, 4.0),
          "source bridge prepares outside measured callback window");
    check(bridge.setPlaybackRate(1.25), "initial source bridge rate accepted");
    check(bridge.setPitchSemitones(0.0f), "neutral source bridge pitch accepted");

    double cursor = 48000.5;
    check(bridge.prime(clip, cursor, false), "source bridge history primes before measurement");

    std::vector<float> left(blockFrames);
    std::vector<float> right(blockFrames);
    for (int block = 0; block < 80; ++block) {
        double next = cursor;
        check(bridge.render(clip, cursor, false, left.data(), right.data(),
                            blockFrames, next),
              "source bridge warmup block renders");
        cursor = next;
    }

    allocations.store(0, std::memory_order_relaxed);
    deallocations.store(0, std::memory_order_relaxed);
    const auto started = std::chrono::steady_clock::now();
    trackHeap.store(true, std::memory_order_seq_cst);
    for (int block = 0; block < blocks; ++block) {
        if ((block % 100) == 0) {
            const int phaseIndex = (block / 100) % 3;
            const double rate = phaseIndex == 0 ? 1.0 : (phaseIndex == 1 ? 1.25 : 1.5);
            if (!bridge.setPlaybackRate(rate)) {
                trackHeap.store(false, std::memory_order_seq_cst);
                throw std::runtime_error("bounded source bridge rate automation accepted");
            }
        }
        double next = cursor;
        if (!bridge.render(clip, cursor, false, left.data(), right.data(),
                           blockFrames, next)) {
            trackHeap.store(false, std::memory_order_seq_cst);
            throw std::runtime_error("source bridge measured block renders");
        }
        cursor = next;
    }
    trackHeap.store(false, std::memory_order_seq_cst);
    const auto finished = std::chrono::steady_clock::now();

    const auto observedAllocations = allocations.load(std::memory_order_relaxed);
    const auto observedDeallocations = deallocations.load(std::memory_order_relaxed);
    check(observedAllocations == 0,
          "source bridge render performs no heap allocation after prepare/prime/warmup");
    check(observedDeallocations == 0,
          "source bridge render performs no heap deallocation after prepare/prime/warmup");
    check(std::all_of(left.begin(), left.end(),
                      [](float value) { return std::isfinite(value); }),
          "source bridge left output remains finite");
    check(std::all_of(right.begin(), right.end(),
                      [](float value) { return std::isfinite(value); }),
          "source bridge right output remains finite");

    const auto elapsedNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started).count();
    const double nsPerFrame =
        static_cast<double>(elapsedNs) / static_cast<double>(blocks * blockFrames);
    check(elapsedNs > 0 && std::isfinite(nsPerFrame) && nsPerFrame > 0.0,
          "source bridge diagnostic processing cost is finite and positive");

    std::cout << "METRIC source_bridge_blocks=" << blocks
              << " ns_per_output_frame=" << nsPerFrame
              << " cursor=" << cursor
              << " source_frames_consumed=" << bridge.sourceFramesConsumed()
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
        std::cout << "PASS: " << checks
                  << " time-stretch source-bridge realtime checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        trackHeap.store(false, std::memory_order_relaxed);
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
