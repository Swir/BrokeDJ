// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
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
    if (trackHeap.load(std::memory_order_relaxed)) allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* pointer = std::malloc(size == 0 ? 1 : size)) return pointer;
    throw std::bad_alloc();
}

void release(void* pointer) noexcept {
    if (pointer != nullptr && trackHeap.load(std::memory_order_relaxed))
        deallocations.fetch_add(1, std::memory_order_relaxed);
    std::free(pointer);
}

std::unique_ptr<broke::Clip> sineClip(int frames = 240000, double sampleRate = 48000.0) {
    auto clip = std::make_unique<broke::Clip>();
    clip->sampleRate = sampleRate;
    clip->left.resize(static_cast<std::size_t>(frames));
    clip->right.resize(static_cast<std::size_t>(frames));
    for (int i = 0; i < frames; ++i) {
        const float value = 0.15f * std::sin(static_cast<float>(i) * 0.017f);
        clip->left[static_cast<std::size_t>(i)] = value;
        clip->right[static_cast<std::size_t>(i)] = -value;
    }
    return clip;
}

std::unique_ptr<broke::Clip> streamedClip() {
    constexpr std::int64_t frames = static_cast<std::int64_t>(broke::StreamCache::chunkFrames) * 8;
    auto cache = std::make_shared<broke::StreamCache>(frames);
    std::array<float, broke::StreamCache::chunkFrames> left{};
    std::array<float, broke::StreamCache::chunkFrames> right{};
    for (std::size_t chunk = 0; chunk < 8; ++chunk) {
        for (std::size_t i = 0; i < left.size(); ++i) {
            const auto absolute = chunk * left.size() + i;
            const float value = 0.1f * std::sin(static_cast<float>(absolute) * 0.021f);
            left[i] = value;
            right[i] = value * 0.7f;
        }
        cache->publishChunk(static_cast<std::int64_t>(chunk), left.data(), right.data(), left.size());
    }
    auto clip = std::make_unique<broke::Clip>();
    clip->sampleRate = 48000.0;
    clip->stream = std::move(cache);
    clip->frameCount = frames;
    return clip;
}

void renderBlock(broke::Engine& engine, std::array<std::array<float, 256>, 4>& audio,
                 std::array<float*, 4>& out) {
    engine.process(out.data(), static_cast<int>(out.size()), static_cast<int>(audio[0].size()));
}

void benchmarkCase(const char* name, double sourceSampleRate, double outputSampleRate, float playbackRate) {
    broke::Engine engine;
    engine.prepare(outputSampleRate);
    engine.crossfader = 0.0f;
    engine.master = 0.8f;
    check(engine.submit(0, sineClip(240000, sourceSampleRate)), "benchmark clip accepted");

    std::array<std::array<float, 256>, 4> audio{};
    std::array<float*, 4> out{};
    for (std::size_t channel = 0; channel < out.size(); ++channel) out[channel] = audio[channel].data();

    renderBlock(engine, audio, out);
    auto& control = engine.control(0);
    control.playing = true;
    control.loop = true;
    control.rate = playbackRate;
    for (int i = 0; i < 64; ++i) renderBlock(engine, audio, out);

    allocations.store(0, std::memory_order_relaxed);
    deallocations.store(0, std::memory_order_relaxed);
    constexpr int blocks = 800;
    const auto started = std::chrono::steady_clock::now();
    trackHeap.store(true, std::memory_order_seq_cst);
    for (int block = 0; block < blocks; ++block) renderBlock(engine, audio, out);
    trackHeap.store(false, std::memory_order_seq_cst);
    const auto finished = std::chrono::steady_clock::now();

    const auto observedAllocations = allocations.load(std::memory_order_relaxed);
    const auto observedDeallocations = deallocations.load(std::memory_order_relaxed);
    check(observedAllocations == 0, "benchmark callback performs no heap allocation");
    check(observedDeallocations == 0, "benchmark callback performs no heap deallocation");
    for (const auto& channel : audio) {
        check(std::all_of(channel.begin(), channel.end(), [](float value) { return std::isfinite(value); }),
              "benchmark callback output remains finite");
    }

    const auto elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started).count();
    constexpr double renderedFrames = static_cast<double>(blocks) * 256.0;
    const double nsPerFrame = static_cast<double>(elapsedNs) / renderedFrames;
    const double frameBudgetNs = 1.0e9 / outputSampleRate;
    const double realtimeBudgetFraction = nsPerFrame / frameBudgetNs;
    check(elapsedNs > 0, "benchmark elapsed time is positive");
    check(std::isfinite(nsPerFrame) && nsPerFrame > 0.0, "benchmark ns/frame is finite and positive");
    check(std::isfinite(realtimeBudgetFraction) && realtimeBudgetFraction > 0.0,
          "benchmark budget fraction is finite and positive");

    std::cout << "METRIC callback_case=" << name
              << " source_hz=" << sourceSampleRate
              << " output_hz=" << outputSampleRate
              << " rate=" << playbackRate
              << " blocks=" << blocks
              << " ns_per_frame=" << nsPerFrame
              << " realtime_budget_fraction=" << realtimeBudgetFraction
              << " heap_allocations=" << observedAllocations
              << " heap_deallocations=" << observedDeallocations
              << " timing_is_diagnostic_only=1\n";
}

void run() {
    broke::Engine engine;
    engine.prepare(48000.0);
    engine.crossfader = 0.35f;
    engine.master = 0.8f;
    engine.headphoneLevel = 0.7f;
    check(engine.submit(0, sineClip()), "in-memory clip accepted");
    check(engine.submit(1, streamedClip()), "streamed clip accepted");

    std::array<std::array<float, 256>, 4> audio{};
    std::array<float*, 4> out{};
    for (std::size_t channel = 0; channel < out.size(); ++channel) out[channel] = audio[channel].data();

    // Adopt clips and let fixed buffers/control smoothers reach normal operation
    // before measuring the callback itself.
    renderBlock(engine, audio, out);
    check(engine.setLoopRegionSeconds(0, 0.25, 1.25), "beat-loop realtime region armed");
    for (std::size_t deck = 0; deck < 2; ++deck) {
        auto& control = engine.control(deck);
        control.playing = true;
        control.loop = true;
        control.headphone = true;
    }
    for (int i = 0; i < 32; ++i) renderBlock(engine, audio, out);

    allocations.store(0, std::memory_order_relaxed);
    deallocations.store(0, std::memory_order_relaxed);
    const auto started = std::chrono::steady_clock::now();
    trackHeap.store(true, std::memory_order_seq_cst);
    for (int block = 0; block < 1200; ++block) {
        if ((block % 37) == 0) {
            auto& a = engine.control(0);
            a.seek = (block % 74) == 0 ? 0.18 : 0.72;
            a.rate = (block % 111) == 0 ? 1.35f : 0.82f;
            a.low = (block % 2) == 0 ? 0.4f : 1.6f;
            a.mid = (block % 3) == 0 ? 0.6f : 1.4f;
            a.high = (block % 5) == 0 ? 0.7f : 1.3f;
            a.echo = (block % 4) == 0 ? 0.55f : 0.1f;
            a.drive = (block % 6) == 0 ? 3.0f : 0.0f;
            a.headphone = !a.headphone.load(std::memory_order_relaxed);
        }
        if ((block % 53) == 0) {
            auto& b = engine.control(1);
            b.seek = (block % 106) == 0 ? 0.03 : 0.61;
            b.rate = (block % 159) == 0 ? 1.2f : 0.95f;
        }
        engine.crossfader = static_cast<float>(block % 101) / 100.0f;
        renderBlock(engine, audio, out);
    }
    trackHeap.store(false, std::memory_order_seq_cst);
    const auto finished = std::chrono::steady_clock::now();

    const auto observedAllocations = allocations.load(std::memory_order_relaxed);
    const auto observedDeallocations = deallocations.load(std::memory_order_relaxed);
    check(observedAllocations == 0, "audio callback performs no heap allocation under transport/FX/beat-loop stress");
    check(observedDeallocations == 0, "audio callback performs no heap deallocation under transport/FX/beat-loop stress");
    for (const auto& channel : audio) {
        check(std::all_of(channel.begin(), channel.end(), [](float value) { return std::isfinite(value); }),
              "callback output remains finite after stress");
    }

    const auto elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started).count();
    constexpr double renderedFrames = 1200.0 * 256.0;
    const double nsPerFrame = static_cast<double>(elapsedNs) / renderedFrames;
    std::cout << "METRIC realtime_blocks=1200 heap_allocations=" << observedAllocations
              << " heap_deallocations=" << observedDeallocations
              << " elapsed_ns=" << elapsedNs
              << " ns_per_rendered_frame=" << nsPerFrame
              << " beat_loop_region_active=1"
              << " timing_is_diagnostic_only=1\n";

    // Release-build CI records these timing diagnostics across representative
    // conversion paths. They are intentionally not absolute pass/fail timing
    // gates because shared runners do not model a user's audio hardware or load.
    benchmarkCase("44k1_to_48k_catmull", 44100.0, 48000.0, 1.0f);
    benchmarkCase("48k_1_5x_sinc", 48000.0, 48000.0, 1.5f);
    benchmarkCase("96k_to_48k_sinc", 96000.0, 48000.0, 1.0f);
    benchmarkCase("192k_to_48k_sinc", 192000.0, 48000.0, 1.0f);
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
        std::cout << "PASS: " << checks << " realtime contract checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        trackHeap.store(false, std::memory_order_relaxed);
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
