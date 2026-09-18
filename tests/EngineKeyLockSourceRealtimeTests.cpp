// SPDX-License-Identifier: AGPL-3.0-only
#include "core/EngineKeyLockDeckOwner.h"
#include "core/EngineKeyLockSource.h"

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
#include <numbers>
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

std::unique_ptr<broke::Clip> makeTone() {
    constexpr double sampleRate = 44100.0;
    constexpr int seconds = 45;
    auto clip = std::make_unique<broke::Clip>();
    clip->sampleRate = sampleRate;
    clip->left.resize(static_cast<std::size_t>(sampleRate * seconds));
    clip->right.resize(clip->left.size());
    for (std::size_t i = 0; i < clip->left.size(); ++i) {
        const double phase = 2.0 * std::numbers::pi * 440.0
            * static_cast<double>(i) / sampleRate;
        clip->left[i] = static_cast<float>(0.12 * std::sin(phase));
        clip->right[i] = clip->left[i] * 0.75f;
    }
    return clip;
}

void runSourceRealtime() {
    using Source = broke::EngineKeyLockSource;
    using Snapshot = Source::ControlSnapshot;
    constexpr double sourceRate = 44100.0;
    constexpr double deviceRate = 48000.0;
    constexpr int frames = 1024;
    constexpr int measuredBlocks = 300;

    broke::Engine engine;
    engine.prepare(deviceRate, frames);
    Source source;
    check(source.prepare(sourceRate, deviceRate, frames, 4.0),
          "production key-lock source prepares off callback");

    auto clip = makeTone();
    const broke::Clip* clipIdentity = clip.get();
    constexpr Snapshot controls{1.25, -2.0f, true};
    check(source.stage(*clipIdentity, 0.0, false, controls),
          "production key-lock source stages off callback");
    check(engine.setDeckSourceRenderer(0, &source),
          "production key-lock source installs before audio starts");
    check(engine.submit(0, std::move(clip)), "production clip submits");

    std::array<std::array<float, frames>, 4> audio{};
    std::array<float*, 4> outputs{};
    for (std::size_t channel = 0; channel < outputs.size(); ++channel)
        outputs[channel] = audio[channel].data();

    engine.process(outputs.data(), 4, frames); // adopt clip while paused
    engine.control(0).rate = static_cast<float>(controls.playbackRate);
    engine.control(0).gain = 1.0f;
    engine.control(0).headphone = true;
    engine.master = 1.0f;
    engine.crossfader = 0.0f;
    engine.headphoneLevel = 1.0f;
    engine.control(0).playing = true;

    for (int block = 0; block < 80; ++block) {
        engine.process(outputs.data(), 4, frames);
        check(source.lastEngineRenderAccepted(), "warmup Engine block accepts staged source");
        check(source.lastRenderPath() == Source::RenderPath::stretch,
              "warmup Engine block remains on stretch path");
    }

    allocations.store(0, std::memory_order_relaxed);
    deallocations.store(0, std::memory_order_relaxed);
    const auto started = std::chrono::steady_clock::now();
    trackHeap.store(true, std::memory_order_seq_cst);
    for (int block = 0; block < measuredBlocks; ++block) {
        engine.process(outputs.data(), 4, frames);
        if (!source.lastEngineRenderAccepted()
            || source.lastRenderPath() != Source::RenderPath::stretch
            || source.lastFallbackReason() != Source::FallbackReason::none
            || !(engine.meter(0).audiblePosition.load() < engine.meter(0).position.load())) {
            trackHeap.store(false, std::memory_order_seq_cst);
            throw std::runtime_error("measured Engine window keeps staged key-lock source and compensated cursor");
        }
    }
    trackHeap.store(false, std::memory_order_seq_cst);
    const auto finished = std::chrono::steady_clock::now();

    const auto observedAllocations = allocations.load(std::memory_order_relaxed);
    const auto observedDeallocations = deallocations.load(std::memory_order_relaxed);
    check(observedAllocations == 0,
          "production Engine/key-lock callback performs no heap allocation after prepare/stage");
    check(observedDeallocations == 0,
          "production Engine/key-lock callback performs no heap deallocation after prepare/stage");
    check(std::all_of(audio[0].begin(), audio[0].end(), [](float value) {
              return std::isfinite(value);
          }) && std::all_of(audio[2].begin(), audio[2].end(), [](float value) {
              return std::isfinite(value);
          }), "master and cue output remain finite in measured callback window");

    const auto elapsedNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started).count();
    const double nsPerFrame = static_cast<double>(elapsedNs)
        / static_cast<double>(measuredBlocks * frames);
    check(elapsedNs > 0 && std::isfinite(nsPerFrame) && nsPerFrame > 0.0,
          "production Engine/key-lock callback timing diagnostic is finite");

    std::cout << "METRIC engine_keylock_blocks=" << measuredBlocks
              << " source_rate=" << sourceRate
              << " device_rate=" << deviceRate
              << " playback_rate=" << controls.playbackRate
              << " pitch_semitones=" << controls.pitchSemitones
              << " latency_frames=" << source.reportedDeviceOutputLatencyFrames()
              << " ns_per_device_frame=" << nsPerFrame
              << " heap_allocations=" << observedAllocations
              << " heap_deallocations=" << observedDeallocations
              << " timing_is_diagnostic_only=1\n";
}

void runOwnerRealtime() {
    using Owner = broke::EngineKeyLockDeckOwner;
    using Snapshot = Owner::ControlSnapshot;
    using Status = Owner::StageStatus;
    constexpr double sourceRate = 44100.0;
    constexpr double deviceRate = 48000.0;
    constexpr int frames = 1024;
    constexpr int measuredBlocks = 220;

    broke::Engine engine;
    engine.prepare(deviceRate, frames);
    Owner owner;
    check(owner.configureDevice(deviceRate, frames, 4.0),
          "deck owner configures fixed device bounds off callback");

    auto clip = makeTone();
    const broke::Clip* clipIdentity = clip.get();
    constexpr Snapshot controls{1.20, 1.0f, true};
    check(owner.stage(*clipIdentity, 0.0, false, controls) == Status::staged,
          "deck owner publishes fully primed snapshot off callback");
    check(engine.setDeckSourceRenderer(0, &owner),
          "deck owner installs once before callback window");
    check(engine.submit(0, std::move(clip)), "deck-owner realtime clip submits");

    std::array<std::array<float, frames>, 4> audio{};
    std::array<float*, 4> outputs{};
    for (std::size_t channel = 0; channel < outputs.size(); ++channel)
        outputs[channel] = audio[channel].data();

    engine.process(outputs.data(), 4, frames); // adopt while paused
    engine.control(0).rate = static_cast<float>(controls.playbackRate);
    engine.control(0).gain = 1.0f;
    engine.control(0).headphone = true;
    engine.master = 1.0f;
    engine.crossfader = 0.0f;
    engine.headphoneLevel = 1.0f;
    engine.control(0).playing = true;

    for (int block = 0; block < 80; ++block) {
        engine.process(outputs.data(), 4, frames);
        check(owner.lastRenderAccepted(), "deck-owner warmup stays on qualified source");
    }

    allocations.store(0, std::memory_order_relaxed);
    deallocations.store(0, std::memory_order_relaxed);
    const auto started = std::chrono::steady_clock::now();
    trackHeap.store(true, std::memory_order_seq_cst);
    for (int block = 0; block < measuredBlocks; ++block) {
        engine.process(outputs.data(), 4, frames);
        if (!owner.lastRenderAccepted()
            || !(engine.meter(0).audiblePosition.load() < engine.meter(0).position.load())) {
            trackHeap.store(false, std::memory_order_seq_cst);
            throw std::runtime_error("deck-owner measured window keeps qualified source and compensated cursor");
        }
    }
    trackHeap.store(false, std::memory_order_seq_cst);
    const auto finished = std::chrono::steady_clock::now();

    const auto observedAllocations = allocations.load(std::memory_order_relaxed);
    const auto observedDeallocations = deallocations.load(std::memory_order_relaxed);
    check(observedAllocations == 0,
          "deck-owner realtime wrapper performs no heap allocation after publication");
    check(observedDeallocations == 0,
          "deck-owner realtime wrapper performs no heap deallocation after publication");
    check(std::all_of(audio[0].begin(), audio[0].end(), [](float value) {
              return std::isfinite(value);
          }) && std::all_of(audio[2].begin(), audio[2].end(), [](float value) {
              return std::isfinite(value);
          }), "deck-owner master and cue output stay finite in measured window");

    const auto elapsedNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started).count();
    const double nsPerFrame = static_cast<double>(elapsedNs)
        / static_cast<double>(measuredBlocks * frames);
    check(elapsedNs > 0 && std::isfinite(nsPerFrame) && nsPerFrame > 0.0,
          "deck-owner callback timing diagnostic is finite");

    std::cout << "METRIC engine_keylock_owner_blocks=" << measuredBlocks
              << " source_rate=" << sourceRate
              << " device_rate=" << deviceRate
              << " playback_rate=" << controls.playbackRate
              << " pitch_semitones=" << controls.pitchSemitones
              << " generation=" << owner.generation()
              << " ns_per_device_frame=" << nsPerFrame
              << " heap_allocations=" << observedAllocations
              << " heap_deallocations=" << observedDeallocations
              << " timing_is_diagnostic_only=1\n";

    check(engine.setDeckSourceRenderer(0, nullptr),
          "deck owner removes after measured callback window");
    owner.resetWhenAudioStopped();
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
        runSourceRealtime();
        runOwnerRealtime();
        std::cout << "PASS: " << checks << " production Engine key-lock realtime checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        trackHeap.store(false, std::memory_order_relaxed);
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
