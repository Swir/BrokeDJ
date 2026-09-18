// SPDX-License-Identifier: AGPL-3.0-only
#include "core/TimeStretch.h"

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

void fillTone(std::array<float, 1280>& left, std::array<float, 1280>& right,
              double& phase, double frequency = 440.0) {
    constexpr double twoPi = 6.28318530717958647692;
    const double advance = twoPi * frequency / 48000.0;
    for (std::size_t i = 0; i < left.size(); ++i) {
        const float value = static_cast<float>(0.2 * std::sin(phase));
        left[i] = value;
        right[i] = value * 0.8f;
        phase += advance;
        if (phase >= twoPi) phase -= twoPi;
    }
}

void run() {
    broke::TimeStretchPrototype processor;
    check(processor.prepare(48000.0, 8192, 4096), "time-stretch prototype prepares");
    check(processor.setPitchSemitones(0.0f), "neutral pitch configured");

    std::array<float, 1280> left{};
    std::array<float, 1280> right{};
    std::array<float, 1024> outLeft{};
    std::array<float, 1024> outRight{};
    double phase = 0.0;

    // Warm all lazily reached code paths before the measured callback-like window.
    for (int block = 0; block < 80; ++block) {
        fillTone(left, right, phase);
        check(processor.processStereo(left.data(), right.data(), static_cast<int>(left.size()),
                                      outLeft.data(), outRight.data(), static_cast<int>(outLeft.size())),
              "warmup block accepted");
    }

    allocations.store(0, std::memory_order_relaxed);
    deallocations.store(0, std::memory_order_relaxed);
    constexpr int blocks = 600;
    const auto started = std::chrono::steady_clock::now();
    trackHeap.store(true, std::memory_order_seq_cst);
    for (int block = 0; block < blocks; ++block) {
        fillTone(left, right, phase);
        if ((block % 100) == 0) {
            const float pitch = (block % 200) == 0 ? 0.0f : 2.0f;
            if (!processor.setPitchSemitones(pitch)) {
                trackHeap.store(false, std::memory_order_seq_cst);
                throw std::runtime_error("bounded pitch automation accepted during measured window");
            }
        }
        if (!processor.processStereo(left.data(), right.data(), static_cast<int>(left.size()),
                                     outLeft.data(), outRight.data(), static_cast<int>(outLeft.size()))) {
            trackHeap.store(false, std::memory_order_seq_cst);
            throw std::runtime_error("measured time-stretch block accepted");
        }
    }
    trackHeap.store(false, std::memory_order_seq_cst);
    const auto finished = std::chrono::steady_clock::now();

    const auto observedAllocations = allocations.load(std::memory_order_relaxed);
    const auto observedDeallocations = deallocations.load(std::memory_order_relaxed);
    check(observedAllocations == 0, "time-stretch processing performs no heap allocation after prepare/warmup");
    check(observedDeallocations == 0, "time-stretch processing performs no heap deallocation after prepare/warmup");
    check(std::all_of(outLeft.begin(), outLeft.end(), [](float value) { return std::isfinite(value); }),
          "left output remains finite");
    check(std::all_of(outRight.begin(), outRight.end(), [](float value) { return std::isfinite(value); }),
          "right output remains finite");

    const auto elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started).count();
    constexpr double outputFrames = static_cast<double>(blocks) * 1024.0;
    const double nsPerOutputFrame = static_cast<double>(elapsedNs) / outputFrames;
    check(elapsedNs > 0, "measured processing time is positive");
    check(std::isfinite(nsPerOutputFrame) && nsPerOutputFrame > 0.0,
          "time-stretch diagnostic cost is finite and positive");

    std::cout << "METRIC timestretch_ratio=1.25 blocks=" << blocks
              << " ns_per_output_frame=" << nsPerOutputFrame
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
        std::cout << "PASS: " << checks << " time-stretch realtime checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        trackHeap.store(false, std::memory_order_relaxed);
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
