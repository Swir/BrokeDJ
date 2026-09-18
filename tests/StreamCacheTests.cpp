// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"
#include <array>
#include <cmath>
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

void run() {
    constexpr std::int64_t total = static_cast<std::int64_t>(broke::StreamCache::chunkFrames) * 3;
    auto cache = std::make_shared<broke::StreamCache>(total);
    std::vector<float> left(broke::StreamCache::chunkFrames, 0.25f);
    std::vector<float> right(broke::StreamCache::chunkFrames, -0.25f);

    check(!cache->hasChunk(0), "empty cache starts unpopulated");
    check(cache->sample(0, 5000) == 0.0f, "missing chunk is silent");
    check(cache->requestedFrame() == 5000, "cache miss requests the missing region");

    cache->publishChunk(1, left.data(), right.data(), left.size());
    check(cache->hasChunk(1), "published chunk becomes visible");
    check(std::abs(cache->sample(0, 5000) - 0.25f) < 0.000001f, "left streamed sample is readable");
    check(std::abs(cache->sample(1, 5000) + 0.25f) < 0.000001f, "right streamed sample is readable");

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
}
}

int main() {
    try {
        run();
        std::cout << "PASS: " << checks << " streaming-cache checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
