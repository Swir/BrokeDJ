// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
int checks = 0;
void check(bool condition, const char* name) {
    ++checks;
    if (!condition) throw std::runtime_error(name);
}

std::vector<float> chunkData(float value) {
    return std::vector<float>(broke::StreamCache::chunkFrames, value);
}

void publishConstant(const std::shared_ptr<broke::StreamCache>& cache,
                     std::int64_t chunk, float value) {
    if (chunk < 0) return;
    const auto start = chunk * static_cast<std::int64_t>(broke::StreamCache::chunkFrames);
    if (start >= cache->totalFrames()) return;
    auto left = chunkData(value);
    auto right = chunkData(-value);
    cache->publishChunk(chunk, left.data(), right.data(), left.size());
}

void publishWindow(const std::shared_ptr<broke::StreamCache>& cache,
                   std::int64_t frame, int before = 1, int ahead = 4) {
    const auto centre = frame / static_cast<std::int64_t>(broke::StreamCache::chunkFrames);
    for (int offset = -before; offset <= ahead; ++offset) {
        const auto chunk = centre + offset;
        const float value = 0.15f + static_cast<float>((chunk >= 0 ? chunk : 0) % 7) * 0.01f;
        publishConstant(cache, chunk, value);
    }
}

void checkFinite(const std::array<std::array<float, 512>, 4>& audio, const char* name) {
    for (const auto& channel : audio)
        for (float sample : channel)
            if (!std::isfinite(sample)) throw std::runtime_error(name);
    ++checks;
}

void runBasicCacheChecks() {
    constexpr std::int64_t total = static_cast<std::int64_t>(broke::StreamCache::chunkFrames) * 3;
    auto cache = std::make_shared<broke::StreamCache>(total);
    auto left = chunkData(0.25f);
    auto right = chunkData(-0.25f);

    check(!cache->hasChunk(0), "empty cache starts unpopulated");
    check(cache->sample(0, 5000) == 0.0f, "missing chunk is silent");
    check(cache->requestedFrame() == 5000, "cache miss requests the missing region");

    auto missing = cache->diagnostics(3);
    check(missing.requestedChunk == 1, "diagnostics identify requested chunk");
    check(!missing.requestedRegionReady, "diagnostics expose missing requested region");
    check(missing.readyChunks == 0 && missing.inspectedChunks == 2,
          "diagnostics bound coverage at end of track");
    check(missing.readMisses == 1 && missing.lastMissFrame == 5000,
          "diagnostics count lock-free cache read misses");
    check(missing.starvationEvents == 0 && !missing.starving,
          "a raw cache probe is not misreported as playback starvation");

    cache->publishChunk(1, left.data(), right.data(), left.size());
    check(cache->hasChunk(1), "published chunk becomes visible");
    check(std::abs(cache->sample(0, 5000) - 0.25f) < 0.000001f, "left streamed sample is readable");
    check(std::abs(cache->sample(1, 5000) + 0.25f) < 0.000001f, "right streamed sample is readable");

    auto recovered = cache->diagnostics(3);
    check(recovered.requestedRegionReady, "diagnostics expose requested-region recovery");
    check(recovered.readyChunks == 1, "diagnostics count resident forward chunks");

    broke::Engine engine;
    engine.prepare(48000.0);
    auto clip = std::make_unique<broke::Clip>();
    clip->sampleRate = 48000.0;
    clip->frameCount = total;
    clip->stream = cache;
    check(clip->valid(), "stream-backed clip is valid");
    check(engine.submit(0, std::move(clip)), "stream-backed clip submits");

    std::array<std::array<float, 512>, 4> audio{};
    std::array<float*, 4> outputs{};
    for (std::size_t i = 0; i < outputs.size(); ++i) outputs[i] = audio[i].data();
    engine.process(outputs.data(), 4, 512); // adopt
    engine.control(0).playing = true;
    engine.control(0).seek = 0.5; // lands in the published middle chunk
    for (int i = 0; i < 4; ++i) engine.process(outputs.data(), 4, 512);
    check(std::isfinite(audio[0][400]), "streamed engine output stays finite");
    check(std::abs(audio[0][400]) > 0.001f, "engine renders cached stream audio");

    engine.control(0).seek = 0.95; // third chunk is absent
    engine.process(outputs.data(), 4, 512);
    check(cache->requestedFrame() >= static_cast<std::int64_t>(broke::StreamCache::chunkFrames) * 2,
          "engine seek requests uncached stream region");
    const auto starved = cache->diagnostics();
    check(!starved.requestedRegionReady,
          "diagnostics report seek target starvation until refill");
    check(starved.starving && starved.starvationEvents >= 1 && starved.readMisses > missing.readMisses,
          "engine classifies continuous cache misses as one starvation episode");
}

void runLongTrackStress() {
    constexpr std::int64_t sampleRate = 48000;
    constexpr std::int64_t total = sampleRate * 60 * 90; // virtual 90-minute track, no full-track allocation
    auto cache = std::make_shared<broke::StreamCache>(total);

    broke::Engine engine;
    engine.prepare(static_cast<double>(sampleRate));
    auto clip = std::make_unique<broke::Clip>();
    clip->sampleRate = static_cast<double>(sampleRate);
    clip->frameCount = total;
    clip->stream = cache;
    check(engine.submit(0, std::move(clip)), "virtual long stream submits without full-track allocation");

    std::array<std::array<float, 512>, 4> audio{};
    std::array<float*, 4> outputs{};
    for (std::size_t i = 0; i < outputs.size(); ++i) outputs[i] = audio[i].data();
    publishWindow(cache, 0);
    engine.process(outputs.data(), 4, 512); // adopt
    engine.control(0).playing = true;

    constexpr std::array<double, 10> seeks {0.01, 0.25, 0.50, 0.90, 0.10, 0.72, 0.33, 0.98, 0.42, 0.66};
    for (double position : seeks) {
        const auto target = static_cast<std::int64_t>(position * static_cast<double>(total - 1));
        publishWindow(cache, target);
        engine.control(0).seek = position;
        for (int block = 0; block < 6; ++block) engine.process(outputs.data(), 4, 512);
        checkFinite(audio, "long-track seek emitted non-finite output");
        const auto diagnostics = cache->diagnostics(6);
        check(diagnostics.requestedFrame >= 0 && diagnostics.requestedFrame < total,
              "long-track request remains in bounds");
        check(diagnostics.requestedRegionReady,
              "preloaded seek window remains resident while rendering");
        check(std::isfinite(engine.meter(0).position.load()),
              "long-track playhead remains finite after repeated seek");
    }

    const auto nearEnd = total - 1024;
    publishWindow(cache, nearEnd, 2, 2);
    publishWindow(cache, 0, 0, 2); // interpolation after wrap needs the start resident too
    engine.control(0).loop = true;
    engine.control(0).seek = static_cast<double>(nearEnd) / static_cast<double>(total - 1);
    for (int block = 0; block < 8; ++block) engine.process(outputs.data(), 4, 512);
    checkFinite(audio, "long-track loop wrap emitted non-finite output");
    check(engine.control(0).playing.load(), "whole-track loop keeps streamed deck playing across wrap");
    check(engine.meter(0).position.load() < 0.2,
          "whole-track loop wraps streamed playhead back near the start");

    constexpr double missingPosition = 0.81;
    engine.control(0).seek = missingPosition;
    engine.process(outputs.data(), 4, 512);
    const auto starved = cache->diagnostics(8);
    check(!starved.requestedRegionReady && starved.starving,
          "diagnostics expose an intentionally unfilled seek starvation episode");
    const auto starvationCount = starved.starvationEvents;
    publishWindow(cache, starved.requestedFrame);
    engine.process(outputs.data(), 4, 512);
    const auto refilled = cache->diagnostics(8);
    check(refilled.requestedRegionReady && refilled.readyChunks >= 1,
          "diagnostics expose requested-region refill after playback resumes");
    check(!refilled.starving && refilled.refillEvents >= 1
              && refilled.starvationEvents == starvationCount,
          "refill closes starvation history without inventing a second episode");
}

void runRefillTransitionCheck() {
    constexpr std::int64_t total = static_cast<std::int64_t>(broke::StreamCache::chunkFrames) * 8;
    auto cache = std::make_shared<broke::StreamCache>(total);

    broke::Engine engine;
    engine.prepare(48000.0);
    engine.master = 1.0f;
    engine.crossfader = 0.0f;
    engine.control(0).gain = 1.0f;

    auto clip = std::make_unique<broke::Clip>();
    clip->sampleRate = 48000.0;
    clip->frameCount = total;
    clip->stream = cache;
    check(engine.submit(0, std::move(clip)), "refill transition stream submits");

    std::array<std::array<float, 512>, 4> audio{};
    std::array<float*, 4> outputs{};
    for (std::size_t i = 0; i < outputs.size(); ++i) outputs[i] = audio[i].data();
    engine.process(outputs.data(), 4, 512); // adopt
    engine.control(0).playing = true;
    for (int block = 0; block < 8; ++block) engine.process(outputs.data(), 4, 512);

    const auto before = cache->diagnostics(4);
    check(before.starving && before.starvationEvents == 1,
          "continuous empty playback records one starvation episode");
    check(before.readMisses > 0, "empty playback records cache read misses");

    publishWindow(cache, before.requestedFrame, 1, 4);
    engine.process(outputs.data(), 4, 512);
    const auto after = cache->diagnostics(4);
    check(!after.starving && after.refillEvents == 1,
          "first fully readable frame closes the starvation episode");
    check(std::abs(audio[0][0]) < 0.01f,
          "refill onset starts from the previous silent state instead of jumping");
    check(std::abs(audio[0][400]) > 0.05f,
          "refill transition reaches audible cached audio after the fade window");
    checkFinite(audio, "refill transition emitted non-finite output");
}
}

int main() {
    try {
        runBasicCacheChecks();
        runLongTrackStress();
        runRefillTransitionCheck();
        std::cout << "PASS: " << checks << " streaming-cache checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
