// SPDX-License-Identifier: AGPL-3.0-only
#include "app/Decoder.h"
#include "core/Engine.h"
#include "core/PerformanceDeckOwner.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
int checks = 0;
constexpr double generatedRate = 44100.0;
constexpr int generatedFrames = 44100 * 6;
constexpr int blockFrames = 512;

void check(bool condition, const char* name) {
    ++checks;
    if (!condition) throw std::runtime_error(name);
}

struct TempFile final {
    TempFile(const juce::String& suffix, const juce::String& stem)
        : file(juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getNonexistentChildFile(stem, suffix, false)) {}
    ~TempFile() { file.deleteFile(); }
    juce::File file;
};

juce::AudioBuffer<float> makeTone() {
    juce::AudioBuffer<float> audio(2, generatedFrames);
    constexpr double twoPi = 6.283185307179586476925286766559;
    for (int i = 0; i < generatedFrames; ++i) {
        const auto t = static_cast<double>(i) / generatedRate;
        const auto carrier = 0.24 * std::sin(twoPi * 330.0 * t);
        const auto overtone = 0.08 * std::sin(twoPi * 990.0 * t + 0.2);
        audio.setSample(0, i, static_cast<float>(carrier + overtone));
        audio.setSample(1, i, static_cast<float>(carrier - overtone));
    }
    return audio;
}

void writeFixture(juce::AudioFormat& format, TempFile& target) {
    target.file.deleteFile();
    auto stream = target.file.createOutputStream();
    check(stream != nullptr, "compressed fixture output stream created");
    const auto possibleRates = format.getPossibleSampleRates();
    const double sampleRate = possibleRates.contains(static_cast<int>(generatedRate))
        ? generatedRate : static_cast<double>(possibleRates.getFirst());
    const auto possibleBits = format.getPossibleBitDepths();
    const int bits = possibleBits.contains(16) ? 16 : possibleBits.getFirst();
    check(sampleRate > 0.0 && bits > 0, "compressed fixture exposes writer settings");

    juce::StringPairArray metadata;
    auto* raw = stream.release();
    const int quality = format.getQualityOptions().isEmpty() ? 0 : format.getQualityOptions().size() / 2;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        format.createWriterFor(raw, sampleRate, 2, bits, metadata, quality));
    if (!writer) {
        delete raw;
        throw std::runtime_error("compressed fixture writer creation failed");
    }
    auto tone = makeTone();
    check(writer->writeFromAudioSampleBuffer(tone, 0, tone.getNumSamples()),
          "compressed fixture write succeeds");
    writer.reset();
    check(target.file.existsAsFile() && target.file.getSize() > 0,
          "compressed fixture exists");
}

bool waitForRegion(const std::shared_ptr<broke::StreamCache>& cache,
                   std::int64_t frame, int timeoutMs) {
    cache->request(frame);
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + static_cast<double>(timeoutMs);
    while (juce::Time::getMillisecondCounterHiRes() < deadline) {
        if (cache->diagnostics(1).requestedRegionReady) return true;
        juce::Thread::sleep(2);
    }
    return cache->diagnostics(1).requestedRegionReady;
}

bool waitForChunk(const std::shared_ptr<broke::StreamCache>& cache,
                  std::int64_t chunk, int timeoutMs) {
    if (chunk < 0) return false;
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + static_cast<double>(timeoutMs);
    while (juce::Time::getMillisecondCounterHiRes() < deadline) {
        if (cache->hasChunk(chunk)) return true;
        juce::Thread::sleep(2);
    }
    return cache->hasChunk(chunk);
}

struct RenderBlock final {
    std::array<std::array<float, blockFrames>, 4> audio{};
    std::array<float*, 4> outputs{};
    RenderBlock() {
        for (std::size_t i = 0; i < outputs.size(); ++i) outputs[i] = audio[i].data();
    }
};

void process(broke::Engine& engine, RenderBlock& block) {
    engine.process(block.outputs.data(), static_cast<int>(block.outputs.size()), blockFrames);
    for (const auto& channel : block.audio)
        for (const auto sample : channel)
            check(std::isfinite(sample), "compressed transport output stays finite");
}

void runInFlightPrefetchPreemption() {
    juce::FlacAudioFormat flac;
    TempFile file(".flac", "BrokeDJ-prefetch-preemption-FLAC");
    writeFixture(flac, file);

    std::atomic<bool> cancelled{false};
    DecodeOptions options;
    options.streamingThresholdBytes = 1;
    options.readAheadDelayMs = 500;
    auto result = decodeTrack(file.file, cancelled, options);
    check(result.error.isEmpty() && result.clip && result.clip->streamed(),
          "preemption fixture uses streaming decoder");

    const auto cache = result.clip->stream;
    constexpr auto chunkFrames = static_cast<std::int64_t>(broke::StreamCache::chunkFrames);
    juce::Thread::sleep(80); // allow the worker to enter the delayed stale chunk-8 fill
    const auto target = chunkFrames * 48 + 32;
    check(target < result.clip->frames(), "preemption target is inside source");
    check(waitForRegion(cache, target, 2500),
          "changed request preempts delayed stale read-ahead and reaches target");
    check(!cache->hasChunk(8),
          "abandoned delayed read-ahead chunk is not published before target recovery");
}

void exerciseCompressedReverseSlip(juce::AudioFormat& format,
                                   const juce::String& suffix,
                                   const juce::String& stem,
                                   const char* label) {
    TempFile file(suffix, stem);
    writeFixture(format, file);

    std::atomic<bool> cancelled{false};
    DecodeOptions options;
    options.streamingThresholdBytes = 1;
    options.readAheadDelayMs = 45;
    auto decoded = decodeTrack(file.file, cancelled, options);
    check(decoded.error.isEmpty() && decoded.clip && decoded.clip->valid()
              && decoded.clip->streamed(),
          "compressed fixture decodes through production streaming adapter");

    auto cache = decoded.clip->stream;
    const auto sourceRate = decoded.clip->sampleRate;
    const auto sourceFrames = decoded.clip->frames();
    const auto targetFrame = sourceFrames * 3 / 4;
    check(waitForRegion(cache, targetFrame, 4000),
          "compressed fixture prepares distant starting region");

    broke::Engine engine;
    engine.prepare(48000.0, blockFrames);
    check(engine.submit(0, std::move(decoded.clip)), "compressed stream submits to Engine");
    RenderBlock block;
    process(engine, block); // adopt

    auto& control = engine.control(0);
    control.seek.store(static_cast<double>(targetFrame) / static_cast<double>(sourceFrames - 1));
    control.playing.store(true);
    process(engine, block);

    broke::PerformanceDeckOwner owner(engine, 0);
    check(owner.setReverseSlipMode(broke::PerformanceDeckOwner::ReverseSlipMode::slipReverse)
              == broke::PerformanceDeckOwner::Result::applied,
          "compressed Slip Reverse arms transactionally");

    const auto beforeDiagnostics = cache->diagnostics();
    const double hiddenStart = engine.meter(0).position.load();
    const double audibleStart = engine.meter(0).audiblePosition.load();

    // Render faster than the intentionally delayed worker can decode. This is
    // deterministic cache starvation evidence, not a physical-disk underrun claim.
    for (int i = 0; i < 360; ++i) process(engine, block);

    const auto starved = cache->diagnostics();
    check(engine.meter(0).position.load() > hiddenStart,
          "hidden compressed Slip timeline keeps moving forward");
    check(engine.meter(0).audiblePosition.load() < audibleStart,
          "audible compressed Slip cursor moves backward");
    check(starved.starvationEvents > beforeDiagnostics.starvationEvents,
          "delayed compressed Reverse/Slip records starvation episode");
    check(starved.starving, "compressed Reverse/Slip exposes active starvation");

    const auto requested = cache->requestedFrame();
    check(waitForRegion(cache, requested, 5000),
          "compressed Reverse/Slip delayed reader refills requested region");

    bool recovered = false;
    for (int i = 0; i < 240; ++i) {
        process(engine, block);
        if (!cache->diagnostics().starving) {
            recovered = true;
            break;
        }
        juce::Thread::sleep(3);
    }
    const auto refilled = cache->diagnostics();
    check(recovered && refilled.refillEvents > beforeDiagnostics.refillEvents,
          "compressed Reverse/Slip closes starvation after refill");

    const auto requestedChunk = cache->requestedFrame()
        / static_cast<std::int64_t>(broke::StreamCache::chunkFrames);
    if (requestedChunk >= 3) {
        check(waitForChunk(cache, requestedChunk - 2, 3000),
              "compressed reverse direction builds read-ahead behind audible cursor");
    }

    check(owner.setReverseSlipMode(broke::PerformanceDeckOwner::ReverseSlipMode::slipArmed)
              == broke::PerformanceDeckOwner::Result::applied,
          "compressed Slip Reverse release keeps Slip armed");
    process(engine, block);
    check(waitForRegion(cache, cache->requestedFrame(), 5000),
          "compressed Slip release prepares hidden timeline region");
    for (int i = 0; i < 8; ++i) process(engine, block);
    check(std::abs(engine.meter(0).audiblePosition.load() - engine.meter(0).position.load()) < 0.08,
          "compressed Slip release rejoins audible and hidden cursors");

    std::cout << "compressed Reverse/Slip OK: " << label << '\n';
}
}

int main() {
    try {
        runInFlightPrefetchPreemption();

        juce::FlacAudioFormat flac;
        exerciseCompressedReverseSlip(flac, ".flac", "BrokeDJ-reverse-slip-FLAC", "FLAC");

        juce::OggVorbisAudioFormat ogg;
        exerciseCompressedReverseSlip(ogg, ".ogg", "BrokeDJ-reverse-slip-OGG", "OGG");

        std::cout << "DecoderTransportStressTests: " << checks << " checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "DecoderTransportStressTests failed after " << checks
                  << " checks: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
