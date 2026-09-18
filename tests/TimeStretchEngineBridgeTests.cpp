// SPDX-License-Identifier: AGPL-3.0-only
#include "core/TimeStretchEngineBridge.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace {
int checks = 0;

void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

broke::Clip makeTone(double sampleRate, double frequency, int seconds = 30) {
    broke::Clip clip;
    clip.sampleRate = sampleRate;
    const auto frames = static_cast<std::size_t>(sampleRate * static_cast<double>(seconds));
    clip.left.resize(frames);
    clip.right.resize(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const double phase = 2.0 * std::numbers::pi * frequency
            * static_cast<double>(i) / sampleRate;
        const float value = static_cast<float>(0.10 * std::sin(phase));
        clip.left[i] = value;
        clip.right[i] = value * 0.8f;
    }
    return clip;
}

std::unique_ptr<broke::Clip> copyClip(const broke::Clip& source) {
    auto result = std::make_unique<broke::Clip>();
    result->sampleRate = source.sampleRate;
    result->left = source.left;
    result->right = source.right;
    result->frameCount = source.frameCount;
    result->stream = source.stream;
    result->sourceOwner = source.sourceOwner;
    return result;
}

void renderEngineBlock(broke::Engine& engine, std::vector<float>& left,
                       std::vector<float>& right) {
    std::array<float*, 2> output {left.data(), right.data()};
    engine.process(output.data(), 2, static_cast<int>(left.size()));
}

void runProductionFallbackParity() {
    constexpr double sourceRate = 96000.0;
    constexpr double deviceRate = 48000.0;
    constexpr double playbackRate = 1.25;
    constexpr int frames = 256;

    auto source = makeTone(sourceRate, 440.0);
    broke::Engine engine;
    engine.prepare(deviceRate);
    engine.crossfader.store(0.0f);
    engine.master.store(1.0f);
    auto& controls = engine.control(0);
    controls.gain.store(1.0f);
    controls.rate.store(static_cast<float>(playbackRate));
    controls.low.store(1.0f);
    controls.mid.store(1.0f);
    controls.high.store(1.0f);
    controls.echo.store(0.0f);
    controls.drive.store(0.0f);
    check(engine.submit(0, copyClip(source)), "production parity clip accepted");

    std::vector<float> engineLeft(frames);
    std::vector<float> engineRight(frames);
    renderEngineBlock(engine, engineLeft, engineRight); // adopt; adoption intentionally stops transport
    controls.playing.store(true);
    for (int block = 0; block < 120; ++block)
        renderEngineBlock(engine, engineLeft, engineRight);

    const double cursor = engine.meter(0).position.load() * sourceRate;
    broke::TimeStretchEngineBridge integration;
    check(integration.prepare(sourceRate, deviceRate, frames, 4.0),
          "Engine-facing bridge prepares");
    check(integration.setPlaybackRate(playbackRate),
          "Engine-facing fallback accepts production playback rate");
    integration.setEnabled(false);

    std::vector<float> bridgeLeft(frames);
    std::vector<float> bridgeRight(frames);
    double nextTransport = cursor;
    double nextAudible = cursor;
    check(integration.render(source, cursor, false,
                             bridgeLeft.data(), bridgeRight.data(), frames,
                             nextTransport, nextAudible),
          "disabled Engine-facing bridge renders production-style fallback");
    renderEngineBlock(engine, engineLeft, engineRight);

    double maxDifference = 0.0;
    for (int frame = 0; frame < frames; ++frame) {
        maxDifference = std::max(maxDifference,
            std::abs(static_cast<double>(bridgeLeft[static_cast<std::size_t>(frame)])
                - static_cast<double>(engineLeft[static_cast<std::size_t>(frame)])));
        maxDifference = std::max(maxDifference,
            std::abs(static_cast<double>(bridgeRight[static_cast<std::size_t>(frame)])
                - static_cast<double>(engineRight[static_cast<std::size_t>(frame)])));
    }
    check(maxDifference < 2.0e-4,
          "parallel fallback stays numerically aligned with settled production Engine source path");
    check(integration.lastRenderPath() == broke::TimeStretchEngineBridge::RenderPath::fallback,
          "disabled integration reports fallback path");
    check(std::abs(nextAudible - nextTransport) < 1.0e-9,
          "fallback scheduling does not invent stretch latency");

    std::cout << "METRIC engine_fallback_parity_max_abs=" << maxDifference << '\n';
}

void runTransitionAndLatencyMatrix() {
    using Integration = broke::TimeStretchEngineBridge;
    using Reason = Integration::FallbackReason;
    using Path = Integration::RenderPath;

    constexpr double sampleRate = 48000.0;
    constexpr int frames = 1024;
    auto clip = makeTone(sampleRate, 330.0, 20);
    auto replacement = makeTone(sampleRate, 550.0, 20);

    Integration integration;
    check(integration.prepare(sampleRate, sampleRate, 4096, 4.0),
          "transition fixture prepares");
    check(integration.setPlaybackRate(1.25), "transition fixture rate accepted");
    check(integration.setPitchSemitones(0.0f), "transition fixture pitch accepted");
    integration.setEnabled(true);

    std::vector<float> left(frames);
    std::vector<float> right(frames);
    double cursor = sampleRate * 2.0 + 0.5;
    check(integration.prime(clip, cursor, false), "transition fixture primes");
    double next = cursor;
    double audible = cursor;
    check(integration.render(clip, cursor, false, left.data(), right.data(), frames, next, audible),
          "primed integration renders");
    check(integration.lastRenderPath() == Path::stretch,
          "primed integration selects stretch path");
    check(integration.reportedDeviceOutputLatencyFrames() > 0,
          "integration exposes nonzero algorithm latency metadata");
    check(audible < next,
          "stretch scheduling applies algorithm latency to audible cursor");
    cursor = next;

    integration.setEnabled(false);
    check(integration.render(clip, cursor, false, left.data(), right.data(), frames, next, audible),
          "bypass renders deterministic fallback");
    check(integration.lastRenderPath() == Path::fallback
              && integration.lastFallbackReason() == Reason::disabled,
          "bypass reports disabled fallback");
    check(std::abs(audible - next) < 1.0e-9,
          "fallback audible cursor equals production transport");
    cursor = next;

    integration.setEnabled(true);
    check(integration.render(clip, cursor, false, left.data(), right.data(), frames, next, audible),
          "re-enable without prime fails closed to fallback");
    check(integration.lastRenderPath() == Path::fallback && integration.needsPrime(),
          "re-enable requires off-callback prime");
    cursor = next;

    check(integration.prime(clip, cursor, false), "re-enable fixture re-primes");
    check(integration.setPlaybackRate(1.10), "changed rate accepted");
    check(integration.needsPrime(), "changed rate invalidates prefetched stretch output");
    check(integration.render(clip, cursor, false, left.data(), right.data(), frames, next, audible),
          "changed rate preserves production fallback");
    check(integration.lastFallbackReason() == Reason::controlChanged,
          "changed rate is diagnosable as control change");
    cursor = next;

    check(integration.setPlaybackRate(1.25), "rate restored");
    check(integration.prime(clip, cursor, false), "pitch fixture primes");
    check(integration.setPitchSemitones(3.0f), "changed pitch accepted");
    check(integration.render(clip, cursor, false, left.data(), right.data(), frames, next, audible),
          "changed pitch preserves production fallback");
    check(integration.lastFallbackReason() == Reason::controlChanged,
          "changed pitch is diagnosable as control change");
    cursor = next;

    check(integration.setPitchSemitones(0.0f), "pitch restored");
    check(integration.prime(clip, cursor, false), "clip replacement fixture primes");
    check(integration.render(clip, cursor, false, left.data(), right.data(), frames, next, audible),
          "clip replacement fixture establishes stretch path");
    cursor = next;
    check(integration.render(replacement, cursor, false,
                             left.data(), right.data(), frames, next, audible),
          "clip replacement fails closed");
    check(integration.lastFallbackReason() == Reason::clipChanged,
          "clip replacement is diagnosable");

    check(integration.prime(clip, cursor, false), "loop fixture primes");
    check(integration.render(clip, cursor, true, left.data(), right.data(), frames, next, audible),
          "loop mode change fails closed");
    check(integration.lastFallbackReason() == Reason::loopModeChanged,
          "loop mode change is diagnosable");

    check(integration.prime(clip, cursor, false), "seek fixture primes");
    const double jumped = cursor + 1500.0;
    check(integration.render(clip, jumped, false, left.data(), right.data(), frames, next, audible),
          "cursor discontinuity fails closed");
    check(integration.lastFallbackReason() == Reason::cursorDiscontinuity,
          "cursor discontinuity is diagnosable");
}

void runTransactionalControlStaging() {
    using Integration = broke::TimeStretchEngineBridge;
    using Snapshot = Integration::ControlSnapshot;
    using Path = Integration::RenderPath;
    using Reason = Integration::FallbackReason;

    constexpr double sampleRate = 48000.0;
    constexpr int frames = 512;
    auto clip = makeTone(sampleRate, 280.0, 20);
    Integration integration;
    check(integration.prepare(sampleRate, sampleRate, 2048, 4.0),
          "transactional fixture prepares");

    double cursor = 5000.5;
    Snapshot first{1.25, 3.0f, true};
    check(integration.configureAndPrime(clip, cursor, false, first),
          "transactional control snapshot stages and primes");
    check(std::abs(integration.playbackRateValue() - first.playbackRate) < 1.0e-12,
          "transactional snapshot publishes playback rate");
    check(std::abs(integration.pitchSemitonesValue() - first.pitchSemitones) < 1.0e-6f,
          "transactional snapshot publishes pitch");
    check(!integration.needsPrime(), "transactional enabled snapshot is ready for render");

    std::vector<float> left(frames);
    std::vector<float> right(frames);
    double next = cursor;
    double audible = cursor;
    check(integration.render(clip, cursor, false, left.data(), right.data(), frames, next, audible),
          "transactional first snapshot renders");
    check(integration.lastRenderPath() == Path::stretch
              && integration.lastFallbackReason() == Reason::none,
          "transactional first snapshot selects stretch immediately");
    cursor = next;

    Snapshot changed{0.80, -2.0f, true};
    check(integration.configureAndPrime(clip, cursor, false, changed),
          "changed rate and pitch are re-staged off callback");
    check(!integration.needsPrime(), "changed staged controls do not leave stale-prime debt");
    check(integration.render(clip, cursor, false, left.data(), right.data(), frames, next, audible),
          "changed staged controls render");
    check(integration.lastRenderPath() == Path::stretch
              && integration.lastFallbackReason() == Reason::none,
          "changed staged controls resume stretch without controlChanged fallback");
    cursor = next;

    Snapshot bypass{1.0, 0.0f, false};
    check(integration.configureAndPrime(clip, cursor, false, bypass),
          "disabled staged snapshot is accepted");
    check(integration.needsPrime(), "disabled staged snapshot intentionally remains unprimed");
    check(integration.render(clip, cursor, false, left.data(), right.data(), frames, next, audible),
          "disabled staged snapshot renders fallback");
    check(integration.lastRenderPath() == Path::fallback
              && integration.lastFallbackReason() == Reason::disabled,
          "disabled staged snapshot is deterministic bypass");
    cursor = next;

    Snapshot invalid{std::numeric_limits<double>::quiet_NaN(), 0.0f, true};
    check(!integration.configureAndPrime(clip, cursor, false, invalid),
          "invalid staged snapshot is rejected");
    check(integration.needsPrime(), "invalid staged snapshot fails closed");
    check(integration.render(clip, cursor, false, left.data(), right.data(), frames, next, audible),
          "invalid staged snapshot still permits safe fallback render");
    check(integration.lastRenderPath() == Path::fallback,
          "invalid staged snapshot cannot select stretch");
    cursor = next;

    Snapshot recovered{1.10, 1.0f, true};
    check(integration.configureAndPrime(clip, cursor, true, recovered),
          "valid staged snapshot recovers after invalid controls");
    check(integration.render(clip, cursor, true, left.data(), right.data(), frames, next, audible),
          "recovered staged snapshot renders");
    check(integration.lastRenderPath() == Path::stretch,
          "recovered staged snapshot returns to stretch path");
}

void runFallbackStreamDiagnostics() {
    using Integration = broke::TimeStretchEngineBridge;
    constexpr double sampleRate = 48000.0;
    constexpr int frames = 256;

    auto cache = std::make_shared<broke::StreamCache>(12000);
    broke::Clip streamClip;
    streamClip.sampleRate = sampleRate;
    streamClip.stream = cache;
    streamClip.frameCount = 12000;
    check(streamClip.valid(), "fallback stream diagnostic clip is valid");

    Integration integration;
    check(integration.prepare(sampleRate, sampleRate, frames, 4.0),
          "fallback stream diagnostic integration prepares");
    check(integration.configureAndPrime(streamClip, 1000.0, false,
                                        Integration::ControlSnapshot{1.0, 0.0f, false}),
          "fallback stream diagnostic bypass config accepted");

    std::vector<float> left(frames);
    std::vector<float> right(frames);
    double next = 1000.0;
    double audible = 1000.0;
    check(integration.render(streamClip, 1000.0, false,
                             left.data(), right.data(), frames, next, audible),
          "uncached production fallback renders bounded silence");
    auto diagnostics = cache->diagnostics();
    check(diagnostics.starving && diagnostics.starvationEvents == 1,
          "production fallback records one stream starvation episode");
    check(diagnostics.readMisses > 0,
          "production fallback retains existing read-miss evidence");

    std::vector<float> chunkLeft(broke::StreamCache::chunkFrames, 0.05f);
    std::vector<float> chunkRight(broke::StreamCache::chunkFrames, 0.04f);
    cache->publishChunk(0, chunkLeft.data(), chunkRight.data(), chunkLeft.size());
    const double secondCursor = next;
    check(integration.render(streamClip, secondCursor, false,
                             left.data(), right.data(), frames, next, audible),
          "resident production fallback renders after refill");
    diagnostics = cache->diagnostics();
    check(!diagnostics.starving && diagnostics.refillEvents == 1,
          "production fallback records refill recovery");
    check(std::any_of(left.begin(), left.end(), [](float value) { return std::abs(value) > 0.001f; }),
          "resident production fallback emits recovered audio");
}

void runStreamFailureFallback() {
    using Integration = broke::TimeStretchEngineBridge;
    using Reason = Integration::FallbackReason;
    using Path = Integration::RenderPath;

    constexpr double sampleRate = 48000.0;
    constexpr int frames = 1024;
    auto cache = std::make_shared<broke::StreamCache>(20000);
    std::vector<float> leftChunk(broke::StreamCache::chunkFrames);
    std::vector<float> rightChunk(broke::StreamCache::chunkFrames);
    for (std::size_t i = 0; i < leftChunk.size(); ++i) {
        const double phase = 2.0 * std::numbers::pi * 220.0
            * static_cast<double>(i) / sampleRate;
        leftChunk[i] = static_cast<float>(0.08 * std::sin(phase));
        rightChunk[i] = leftChunk[i] * 0.7f;
    }
    cache->publishChunk(0, leftChunk.data(), rightChunk.data(), leftChunk.size());
    cache->publishChunk(1, leftChunk.data(), rightChunk.data(), leftChunk.size());

    broke::Clip streamClip;
    streamClip.sampleRate = sampleRate;
    streamClip.stream = cache;
    streamClip.frameCount = 20000;
    check(streamClip.valid(), "stream failure fixture clip is valid");

    Integration integration;
    check(integration.prepare(sampleRate, sampleRate, frames, 4.0),
          "stream failure integration prepares");
    check(integration.setPlaybackRate(1.0), "stream failure rate accepted");
    integration.setEnabled(true);
    double cursor = 6500.0;
    check(integration.prime(streamClip, cursor, false),
          "stream failure fixture primes from resident history");

    std::vector<float> left(frames);
    std::vector<float> right(frames);
    bool observedFallback = false;
    for (int block = 0; block < 6; ++block) {
        double next = cursor;
        double audible = cursor;
        check(integration.render(streamClip, cursor, false,
                                 left.data(), right.data(), frames, next, audible),
              "stream failure fixture renders bounded block");
        cursor = next;
        if (integration.lastRenderPath() == Path::fallback) {
            observedFallback = true;
            break;
        }
    }
    check(observedFallback, "uncached stream region fails closed to production fallback");
    check(integration.lastFallbackReason() == Reason::stretchFailure,
          "stream/source render failure maps to deterministic stretchFailure reason");
    check(integration.needsPrime(), "render failure requires explicit off-callback re-prime");
    check(cache->diagnostics().readMisses > 0,
          "stream failure remains visible through existing cache diagnostics");
}

void run() {
    runProductionFallbackParity();
    runTransitionAndLatencyMatrix();
    runTransactionalControlStaging();
    runFallbackStreamDiagnostics();
    runStreamFailureFallback();
}
} // namespace

int main() {
    try {
        run();
        std::cout << "PASS: " << checks << " Engine-facing time-stretch integration checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
