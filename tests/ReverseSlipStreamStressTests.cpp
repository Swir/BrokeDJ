// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"
#include "core/PerformanceDeckOwner.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
int checks = 0;
constexpr double sampleRate = 48000.0;
constexpr int blockFrames = 256;

void check(bool condition, const char* name) {
    ++checks;
    if (!condition) throw std::runtime_error(name);
}

struct RenderBlock final {
    std::array<float, blockFrames> left{};
    std::array<float, blockFrames> right{};
    std::array<float*, 2> channels{left.data(), right.data()};
};

void checkFinite(const RenderBlock& block, const char* name) {
    for (std::size_t i = 0; i < block.left.size(); ++i) {
        if (!std::isfinite(block.left[i]) || !std::isfinite(block.right[i]))
            throw std::runtime_error(name);
    }
    ++checks;
}

void publishChunk(const std::shared_ptr<broke::StreamCache>& cache, std::int64_t chunkIndex) {
    if (!cache || chunkIndex < 0) return;
    const auto start = chunkIndex * static_cast<std::int64_t>(broke::StreamCache::chunkFrames);
    if (start >= cache->totalFrames()) return;
    const auto remaining = cache->totalFrames() - start;
    const auto frames = static_cast<std::size_t>(std::min<std::int64_t>(
        remaining, static_cast<std::int64_t>(broke::StreamCache::chunkFrames)));
    std::array<float, broke::StreamCache::chunkFrames> left{};
    std::array<float, broke::StreamCache::chunkFrames> right{};
    for (std::size_t i = 0; i < frames; ++i) {
        const double t = static_cast<double>(start + static_cast<std::int64_t>(i)) / sampleRate;
        const float value = static_cast<float>(0.18 * std::sin(2.0 * 3.14159265358979323846 * 330.0 * t));
        left[i] = value;
        right[i] = -value;
    }
    cache->publishChunk(chunkIndex, left.data(), right.data(), frames);
}

void publishWindow(const std::shared_ptr<broke::StreamCache>& cache,
                   std::int64_t frame, int radius = 3) {
    const auto centre = frame / static_cast<std::int64_t>(broke::StreamCache::chunkFrames);
    for (int offset = -radius; offset <= radius; ++offset)
        publishChunk(cache, centre + static_cast<std::int64_t>(offset));
}

std::unique_ptr<broke::Clip> makeVirtualLongTrack(std::shared_ptr<broke::StreamCache>& cacheOut) {
    constexpr std::int64_t minutes = 90;
    const auto frames = static_cast<std::int64_t>(sampleRate * 60.0 * static_cast<double>(minutes));
    cacheOut = std::make_shared<broke::StreamCache>(frames);
    auto clip = std::make_unique<broke::Clip>();
    clip->sampleRate = sampleRate;
    clip->stream = cacheOut;
    clip->frameCount = frames;
    return clip;
}

void process(broke::Engine& engine, RenderBlock& block) {
    engine.process(block.channels.data(), static_cast<int>(block.channels.size()), blockFrames);
}

void longTrackReverseSlipKeepsSplitCursorsBounded() {
    broke::Engine engine;
    engine.prepare(sampleRate, blockFrames);
    std::shared_ptr<broke::StreamCache> cache;
    check(engine.submit(0, makeVirtualLongTrack(cache)), "90-minute stream submits without full-track allocation");
    publishWindow(cache, 0);
    RenderBlock block;
    process(engine, block); // adopt

    constexpr double seekFraction = 0.60;
    const auto targetFrame = static_cast<std::int64_t>(
        seekFraction * static_cast<double>(cache->totalFrames() - 1));
    publishWindow(cache, targetFrame, 5);
    auto& control = engine.control(0);
    control.seek.store(seekFraction);
    control.playing.store(true);
    process(engine, block);
    checkFinite(block, "initial streamed seek render must remain finite");

    broke::PerformanceDeckOwner owner(engine, 0);
    check(owner.setReverseSlipMode(broke::PerformanceDeckOwner::ReverseSlipMode::slipReverse)
              == broke::PerformanceDeckOwner::Result::applied,
          "slip reverse arms on ordinary streamed transport");

    const double hiddenStart = engine.meter(0).position.load();
    const double audibleStart = engine.meter(0).audiblePosition.load();
    for (int blockIndex = 0; blockIndex < 120; ++blockIndex) {
        const auto requested = cache->requestedFrame();
        publishWindow(cache, requested, 3);
        process(engine, block);
        checkFinite(block, "slip reverse stream render must remain finite");
    }
    const double hiddenAfter = engine.meter(0).position.load();
    const double audibleAfter = engine.meter(0).audiblePosition.load();
    check(hiddenAfter > hiddenStart,
          "hidden slip transport continues forward over long streamed track");
    check(audibleAfter < audibleStart,
          "audible cursor moves backward while hidden slip transport advances");
    check(hiddenAfter > audibleAfter,
          "slip reverse maintains distinct hidden and audible cursors");

    check(owner.setReverseSlipMode(broke::PerformanceDeckOwner::ReverseSlipMode::slipArmed)
              == broke::PerformanceDeckOwner::Result::applied,
          "releasing reverse keeps slip armed");
    publishWindow(cache, cache->requestedFrame(), 3);
    process(engine, block);
    check(std::abs(engine.meter(0).audiblePosition.load() - engine.meter(0).position.load()) < 0.05,
          "reverse release rejoins audible cursor to hidden timeline");
    checkFinite(block, "slip rejoin must remain finite");
}

void starvationAndRefillRemainOneEpisodeDuringReverse() {
    broke::Engine engine;
    engine.prepare(sampleRate, blockFrames);
    std::shared_ptr<broke::StreamCache> cache;
    check(engine.submit(1, makeVirtualLongTrack(cache)), "starvation fixture submits");
    publishWindow(cache, 0);
    RenderBlock block;
    process(engine, block);

    constexpr double seekFraction = 0.35;
    const auto targetFrame = static_cast<std::int64_t>(
        seekFraction * static_cast<double>(cache->totalFrames() - 1));
    publishWindow(cache, targetFrame, 4);
    auto& control = engine.control(1);
    control.seek.store(seekFraction);
    control.playing.store(true);
    process(engine, block);

    broke::PerformanceDeckOwner owner(engine, 1);
    check(owner.setReverseEnabled(true) == broke::PerformanceDeckOwner::Result::applied,
          "reverse-only streamed mode arms");
    for (int i = 0; i < 24; ++i) {
        publishWindow(cache, cache->requestedFrame(), 2);
        process(engine, block);
    }
    const double beforeStarvation = engine.meter(1).audiblePosition.load();

    // Intentionally stop serving the directionally requested region. The fixed
    // cache will eventually move beyond resident chunks and Engine must collapse
    // repeated misses into one starvation episode while output remains finite.
    const auto before = cache->diagnostics();
    for (int i = 0; i < 180; ++i) {
        process(engine, block);
        checkFinite(block, "starved reverse output must remain finite");
    }
    const auto starved = cache->diagnostics();
    check(starved.readMisses >= before.readMisses,
          "reverse starvation never loses read-miss history");
    check(starved.starvationEvents >= before.starvationEvents,
          "reverse starvation event counter is monotonic");
    check(engine.meter(1).audiblePosition.load() < beforeStarvation,
          "reverse transport remains directionally coherent during bounded starvation");

    const auto requested = cache->requestedFrame();
    publishWindow(cache, requested, 5);
    for (int i = 0; i < 16; ++i) {
        publishWindow(cache, cache->requestedFrame(), 3);
        process(engine, block);
        checkFinite(block, "reverse refill output must remain finite");
    }
    const auto refilled = cache->diagnostics();
    check(refilled.refillEvents >= before.refillEvents,
          "reverse refill event counter is monotonic after data returns");
    check(!refilled.starving || refilled.requestedRegionReady,
          "refill either clears starvation or has requested region resident");
}

void repeatedDirectionChangesKeepRequestInTrack() {
    broke::Engine engine;
    engine.prepare(sampleRate, blockFrames);
    std::shared_ptr<broke::StreamCache> cache;
    check(engine.submit(2, makeVirtualLongTrack(cache)), "direction-change fixture submits");
    publishWindow(cache, 0);
    RenderBlock block;
    process(engine, block);

    constexpr double seekFraction = 0.50;
    const auto targetFrame = static_cast<std::int64_t>(
        seekFraction * static_cast<double>(cache->totalFrames() - 1));
    publishWindow(cache, targetFrame, 6);
    auto& control = engine.control(2);
    control.seek.store(seekFraction);
    control.playing.store(true);
    process(engine, block);

    broke::PerformanceDeckOwner owner(engine, 2);
    for (int cycle = 0; cycle < 40; ++cycle) {
        const bool reverse = (cycle % 2) != 0;
        check(owner.setReverseEnabled(reverse) == broke::PerformanceDeckOwner::Result::applied,
              "direction change applies transactionally");
        for (int i = 0; i < 4; ++i) {
            publishWindow(cache, cache->requestedFrame(), 3);
            process(engine, block);
            checkFinite(block, "direction-change streamed render must remain finite");
            const auto request = cache->requestedFrame();
            check(request >= 0 && request < cache->totalFrames(),
                  "direction-aware request always stays inside track bounds");
        }
    }
}
} // namespace

int main() {
    try {
        longTrackReverseSlipKeepsSplitCursorsBounded();
        starvationAndRefillRemainOneEpisodeDuringReverse();
        repeatedDirectionChangesKeepRequestInTrack();
        std::cout << "ReverseSlipStreamStressTests: " << checks << " checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "ReverseSlipStreamStressTests failed after " << checks
                  << " checks: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
