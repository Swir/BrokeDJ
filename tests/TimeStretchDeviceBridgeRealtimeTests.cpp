// SPDX-License-Identifier: AGPL-3.0-only
#include "core/TimeStretchDeviceBridge.h"

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
    constexpr double sourceRate = 44100.0;
    constexpr int seconds = 30;
    broke::Clip clip;
    clip.sampleRate = sourceRate;
    clip.left.resize(static_cast<std::size_t>(sourceRate * seconds));
    clip.right.resize(clip.left.size());
    for (std::size_t i = 0; i < clip.left.size(); ++i) {
        const double phase = 2.0 * std::numbers::pi * 440.0
            * static_cast<double>(i) / sourceRate;
        clip.left[i] = static_cast<float>(0.14 * std::sin(phase));
        clip.right[i] = clip.left[i] * 0.75f;
    }
    return clip;
}

void run() {
    constexpr double sourceRate = 44100.0;
    constexpr double deviceRate = 48000.0;
    constexpr double playbackRate = 1.25;
    constexpr int blockFrames = 1024;
    constexpr int measuredBlocks = 600;

    auto clip = makeToneClip();
    broke::TimeStretchDeviceBridge bridge;
    check(bridge.prepare(sourceRate, deviceRate, 4096, 4.0),
          "device bridge prepares outside measured callback window");
    check(bridge.setPlaybackRate(playbackRate), "device bridge playback rate accepted");
    check(bridge.setPitchSemitones(0.0f), "device bridge neutral pitch accepted");
    bridge.setEnabled(true);

    double cursor = sourceRate + 0.5;
    check(bridge.prime(clip, cursor, false), "device bridge primes before measurement");

    std::vector<float> fallbackLeft(blockFrames, 0.0f);
    std::vector<float> fallbackRight(blockFrames, 0.0f);
    std::vector<float> left(blockFrames);
    std::vector<float> right(blockFrames);
    const double advance = static_cast<double>(blockFrames)
        * (sourceRate / deviceRate) * playbackRate;

    for (int block = 0; block < 80; ++block) {
        double next = cursor;
        check(bridge.render(clip, cursor, false,
                            fallbackLeft.data(), fallbackRight.data(), cursor + advance,
                            left.data(), right.data(), blockFrames, next),
              "device bridge warmup renders");
        check(bridge.lastRenderPath() == broke::TimeStretchDeviceBridge::RenderPath::stretch,
              "warmup stays on stretch path");
        cursor = next;
    }

    allocations.store(0, std::memory_order_relaxed);
    deallocations.store(0, std::memory_order_relaxed);
    const auto started = std::chrono::steady_clock::now();
    trackHeap.store(true, std::memory_order_seq_cst);
    for (int block = 0; block < measuredBlocks; ++block) {
        // Model a future Engine callback pushing an unchanged atomic control
        // snapshot every block. Re-applying equal values must remain bounded,
        // allocation-free and must not invalidate already prepared history.
        if (!bridge.setPlaybackRate(playbackRate)
            || !bridge.setPitchSemitones(0.0f)
            || bridge.needsPrime()) {
            trackHeap.store(false, std::memory_order_seq_cst);
            throw std::runtime_error("idempotent realtime control snapshot keeps bridge primed");
        }
        double next = cursor;
        if (!bridge.render(clip, cursor, false,
                           fallbackLeft.data(), fallbackRight.data(), cursor + advance,
                           left.data(), right.data(), blockFrames, next)
            || bridge.lastRenderPath() != broke::TimeStretchDeviceBridge::RenderPath::stretch
            || bridge.lastFallbackReason() != broke::TimeStretchDeviceBridge::FallbackReason::none) {
            trackHeap.store(false, std::memory_order_seq_cst);
            throw std::runtime_error("device bridge measured block stays on stretch path");
        }
        cursor = next;
    }
    trackHeap.store(false, std::memory_order_seq_cst);
    const auto finished = std::chrono::steady_clock::now();

    const auto observedAllocations = allocations.load(std::memory_order_relaxed);
    const auto observedDeallocations = deallocations.load(std::memory_order_relaxed);
    check(observedAllocations == 0,
          "device bridge render/control snapshot performs no heap allocation after prepare/prime/warmup");
    check(observedDeallocations == 0,
          "device bridge render/control snapshot performs no heap deallocation after prepare/prime/warmup");
    check(std::all_of(left.begin(), left.end(), [](float value) { return std::isfinite(value); }),
          "device bridge left output remains finite");
    check(std::all_of(right.begin(), right.end(), [](float value) { return std::isfinite(value); }),
          "device bridge right output remains finite");

    const auto elapsedNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started).count();
    const double nsPerDeviceFrame = static_cast<double>(elapsedNs)
        / static_cast<double>(measuredBlocks * blockFrames);
    check(elapsedNs > 0 && std::isfinite(nsPerDeviceFrame) && nsPerDeviceFrame > 0.0,
          "device bridge diagnostic processing cost is finite and positive");

    std::cout << "METRIC device_bridge_blocks=" << measuredBlocks
              << " source_rate=" << sourceRate
              << " device_rate=" << deviceRate
              << " playback_rate=" << playbackRate
              << " ns_per_device_frame=" << nsPerDeviceFrame
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
        std::cout << "PASS: " << checks << " time-stretch device-bridge realtime checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        trackHeap.store(false, std::memory_order_relaxed);
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
