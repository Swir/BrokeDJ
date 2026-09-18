// SPDX-License-Identifier: AGPL-3.0-only
#include "core/TimeStretchSourceBridge.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace {
constexpr double sampleRate = 48000.0;
constexpr int outputFrames = 1024;
int checks = 0;

void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

broke::Clip makeToneClip(double frequency = 440.0, int seconds = 12,
                         double rate = sampleRate) {
    broke::Clip clip;
    clip.sampleRate = rate;
    const auto frames = static_cast<std::size_t>(rate * static_cast<double>(seconds));
    clip.left.resize(frames);
    clip.right.resize(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const auto phase = 2.0 * std::numbers::pi * frequency
            * static_cast<double>(i) / rate;
        const float value = static_cast<float>(0.2 * std::sin(phase));
        clip.left[i] = value;
        clip.right[i] = value * 0.8f;
    }
    return clip;
}

double estimateFrequency(const std::vector<float>& samples) {
    std::size_t crossings = 0;
    for (std::size_t i = 1; i < samples.size(); ++i)
        if (samples[i - 1] <= 0.0f && samples[i] > 0.0f) ++crossings;
    const double duration = samples.size() > 1
        ? static_cast<double>(samples.size() - 1) / sampleRate : 0.0;
    return duration > 0.0 ? static_cast<double>(crossings) / duration : 0.0;
}

void publishToneChunk(const std::shared_ptr<broke::StreamCache>& cache,
                      std::int64_t chunk, double frequency = 330.0) {
    if (chunk < 0) return;
    const auto start = chunk * static_cast<std::int64_t>(broke::StreamCache::chunkFrames);
    if (start >= cache->totalFrames()) return;
    std::vector<float> left(broke::StreamCache::chunkFrames);
    std::vector<float> right(broke::StreamCache::chunkFrames);
    for (std::size_t i = 0; i < left.size(); ++i) {
        const auto absolute = start + static_cast<std::int64_t>(i);
        const auto phase = 2.0 * std::numbers::pi * frequency
            * static_cast<double>(absolute) / sampleRate;
        left[i] = static_cast<float>(0.16 * std::sin(phase));
        right[i] = left[i] * 0.7f;
    }
    cache->publishChunk(chunk, left.data(), right.data(), left.size());
}

void run() {
    broke::TimeStretchSourceBridge bridge;
    check(!bridge.prepare(0.0, 4096), "invalid source rate rejected");
    check(bridge.prepare(sampleRate, 4096, 4.0), "source bridge prepares");
    check(bridge.maxInputFrames() >= 16384, "source bridge owns bounded worst-case input scratch");
    check(bridge.seekLengthFrames() > 0
              && bridge.seekLengthFrames() <= bridge.maxInputFrames(),
          "source bridge seek history fits prepared scratch");
    check(bridge.setPlaybackRate(1.25), "1.25x source rate accepted");
    check(bridge.setPitchSemitones(0.0f), "neutral pitch accepted");

    auto clip = makeToneClip();
    double cursor = sampleRate + 0.375;
    check(bridge.prime(clip, cursor, false), "in-memory source history primes");
    check(bridge.primed(), "bridge exposes primed state");

    std::vector<float> left(outputFrames);
    std::vector<float> right(outputFrames);
    std::vector<float> captured;
    captured.reserve(static_cast<std::size_t>(80 * outputFrames));
    const double startCursor = cursor;
    std::int64_t plannedSourceFrames = 0;
    for (int block = 0; block < 120; ++block) {
        const auto before = bridge.sourceFramesConsumed();
        double next = cursor;
        check(bridge.render(clip, cursor, false, left.data(), right.data(),
                            outputFrames, next),
              "bounded in-memory source block renders");
        plannedSourceFrames += bridge.sourceFramesConsumed() - before;
        check(next > cursor, "successful source bridge render advances cursor");
        cursor = next;
        if (block >= 40) captured.insert(captured.end(), left.begin(), left.end());
    }
    const double lockedHz = estimateFrequency(captured);
    check(std::abs(lockedHz - 440.0) < 5.0,
          "1.25x source bridge preserves source pitch");
    check(std::abs((cursor - startCursor) - static_cast<double>(plannedSourceFrames)) < 1.0e-9,
          "transport cursor matches bounded source-frame consumption");
    check(std::all_of(captured.begin(), captured.end(),
                      [](float value) { return std::isfinite(value); }),
          "source bridge output remains finite");

    auto wrongRate = makeToneClip(440.0, 4, 44100.0);
    double unchanged = cursor;
    check(!bridge.render(wrongRate, cursor, false, left.data(), right.data(),
                         outputFrames, unchanged),
          "source-rate mismatch fails closed for production fallback");
    check(unchanged == cursor, "source-rate mismatch does not advance transport");

    check(bridge.setPlaybackRate(1.0), "neutral rate restored");
    const double nearEnd = static_cast<double>(clip.frames()) - 200.0;
    check(bridge.prime(clip, nearEnd, false), "near-EOF history primes");
    unchanged = nearEnd;
    check(!bridge.render(clip, nearEnd, false, left.data(), right.data(),
                         outputFrames, unchanged),
          "non-looping block that would cross EOF fails closed");
    check(unchanged == nearEnd, "EOF fallback leaves transport untouched");

    check(bridge.prime(clip, nearEnd, true), "looping near-EOF history primes");
    double wrapped = nearEnd;
    check(bridge.render(clip, nearEnd, true, left.data(), right.data(),
                        outputFrames, wrapped),
          "looping source bridge renders across EOF");
    check(wrapped >= 0.0 && wrapped < static_cast<double>(clip.frames()),
          "looping source bridge wraps next cursor into clip bounds");

    constexpr std::int64_t streamFrames =
        static_cast<std::int64_t>(broke::StreamCache::chunkFrames) * 5;
    auto cache = std::make_shared<broke::StreamCache>(streamFrames);
    publishToneChunk(cache, 0);
    publishToneChunk(cache, 1);

    broke::Clip streamClip;
    streamClip.sampleRate = sampleRate;
    streamClip.frameCount = streamFrames;
    streamClip.stream = cache;
    check(streamClip.valid(), "stream-backed bridge fixture is valid");

    check(bridge.setPlaybackRate(1.25), "stream fixture rate accepted");
    const double streamCursor =
        static_cast<double>(broke::StreamCache::chunkFrames * 2 - 256);
    check(bridge.prime(streamClip, streamCursor, false),
          "resident stream history primes before chunk boundary");

    double streamNext = streamCursor;
    const auto consumedBeforeStarve = bridge.sourceFramesConsumed();
    check(!bridge.render(streamClip, streamCursor, false, left.data(), right.data(),
                         outputFrames, streamNext),
          "missing future stream chunk fails closed instead of mixing partial data");
    check(streamNext == streamCursor, "stream starvation leaves transport cursor untouched");
    check(bridge.sourceFramesConsumed() == consumedBeforeStarve,
          "stream starvation leaves time-stretch source clock untouched");
    const auto starved = cache->diagnostics();
    check(starved.starving && starved.starvationEvents >= 1,
          "stream bridge records one playback starvation episode");

    publishToneChunk(cache, 2);
    streamNext = streamCursor;
    check(bridge.render(streamClip, streamCursor, false, left.data(), right.data(),
                        outputFrames, streamNext),
          "refilled stream block resumes through the source bridge");
    const auto refilled = cache->diagnostics();
    check(!refilled.starving && refilled.refillEvents >= 1,
          "successful streamed render closes starvation episode");
    check(std::abs(left.front()) < 0.01f,
          "stream recovery begins with prepared entry fade");
    check(streamNext > streamCursor, "refilled stream render advances transport");

    std::cout << "METRIC source_bridge_keylock_1_25x_hz=" << lockedHz
              << " source_frames=" << plannedSourceFrames
              << " stream_starvations=" << refilled.starvationEvents
              << " stream_refills=" << refilled.refillEvents
              << " input_latency_frames=" << bridge.inputLatencyFrames()
              << " output_latency_frames=" << bridge.outputLatencyFrames() << '\n';
}
} // namespace

int main() {
    try {
        run();
        std::cout << "PASS: " << checks << " time-stretch source-bridge checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
