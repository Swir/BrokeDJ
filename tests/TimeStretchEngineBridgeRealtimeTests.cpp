// SPDX-License-Identifier: AGPL-3.0-only
#include "core/TimeStretchEngineBridge.h"

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

broke::Clip makeTone() {
    constexpr double sampleRate = 44100.0;
    constexpr int seconds = 45;
    broke::Clip clip;
    clip.sampleRate = sampleRate;
    clip.left.resize(static_cast<std::size_t>(sampleRate * seconds));
    clip.right.resize(clip.left.size());
    for (std::size_t i = 0; i < clip.left.size(); ++i) {
        const double phase = 2.0 * std::numbers::pi * 440.0
            * static_cast<double>(i) / sampleRate;
        clip.left[i] = static_cast<float>(0.12 * std::sin(phase));
        clip.right[i] = clip.left[i] * 0.75f;
    }
    return clip;
}

void run() {
    constexpr double sourceRate = 44100.0;
    constexpr double deviceRate = 48000.0;
    constexpr double playbackRate = 1.25;
    constexpr int frames = 1024;
    constexpr int measuredBlocks = 500;

    auto clip = makeTone();
    broke::TimeStretchEngineBridge integration;
    check(integration.prepare(sourceRate, deviceRate, 4096, 4.0),
          "Engine-facing integration prepares outside realtime window");
    check(integration.setPlaybackRate(playbackRate), "playback rate accepted");
    check(integration.setPitchSemitones(0.0f), "neutral pitch accepted");
    integration.setEnabled(true);

    double cursor = sourceRate * 2.0 + 0.5;
    check(integration.prime(clip, cursor, false), "integration primes before measurement");
    std::vector<float> left(frames);
    std::vector<float> right(frames);

    for (int block = 0; block < 80; ++block) {
        double next = cursor;
        double audible = cursor;
        check(integration.render(clip, cursor, false,
                                 left.data(), right.data(), frames, next, audible),
              "warmup block renders");
        check(integration.lastRenderPath() == broke::TimeStretchEngineBridge::RenderPath::stretch,
              "warmup remains on stretch path");
        cursor = next;
    }

    allocations.store(0, std::memory_order_relaxed);
    deallocations.store(0, std::memory_order_relaxed);
    const auto started = std::chrono::steady_clock::now();
    trackHeap.store(true, std::memory_order_seq_cst);
    for (int block = 0; block < measuredBlocks; ++block) {
        if (!integration.setPlaybackRate(playbackRate)
            || !integration.setPitchSemitones(0.0f)
            || integration.needsPrime()) {
            trackHeap.store(false, std::memory_order_seq_cst);
            throw std::runtime_error("idempotent Engine control snapshot remains primed");
        }
        double next = cursor;
        double audible = cursor;
        if (!integration.render(clip, cursor, false,
                                left.data(), right.data(), frames, next, audible)
            || integration.lastRenderPath() != broke::TimeStretchEngineBridge::RenderPath::stretch
            || integration.lastFallbackReason() != broke::TimeStretchEngineBridge::FallbackReason::none
            || !(audible < next)) {
            trackHeap.store(false, std::memory_order_seq_cst);
            throw std::runtime_error("measured block keeps stretch path and compensated scheduling");
        }
        cursor = next;
    }
    trackHeap.store(false, std::memory_order_seq_cst);
    const auto finished = std::chrono::steady_clock::now();

    const auto observedAllocations = allocations.load(std::memory_order_relaxed);
    const auto observedDeallocations = deallocations.load(std::memory_order_relaxed);
    check(observedAllocations == 0,
          "Engine-facing render/control snapshot performs no heap allocation after prepare/prime");
    check(observedDeallocations == 0,
          "Engine-facing render/control snapshot performs no heap deallocation after prepare/prime");
    check(std::all_of(left.begin(), left.end(), [](float value) { return std::isfinite(value); }),
          "left output remains finite");
    check(std::all_of(right.begin(), right.end(), [](float value) { return std::isfinite(value); }),
          "right output remains finite");

    const auto elapsedNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started).count();
    const double nsPerFrame = static_cast<double>(elapsedNs)
        / static_cast<double>(measuredBlocks * frames);
    check(elapsedNs > 0 && std::isfinite(nsPerFrame) && nsPerFrame > 0.0,
          "Engine-facing diagnostic callback cost is finite and positive");

    std::cout << "METRIC engine_bridge_blocks=" << measuredBlocks
              << " source_rate=" << sourceRate
              << " device_rate=" << deviceRate
              << " playback_rate=" << playbackRate
              << " latency_frames=" << integration.reportedDeviceOutputLatencyFrames()
              << " ns_per_device_frame=" << nsPerFrame
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
        std::cout << "PASS: " << checks << " Engine-facing realtime integration checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        trackHeap.store(false, std::memory_order_relaxed);
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
