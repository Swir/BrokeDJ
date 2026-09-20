// SPDX-License-Identifier: AGPL-3.0-only
#include "app/Decoder.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
int checks = 0;
void check(bool condition, const char* name) {
    ++checks;
    if (!condition) throw std::runtime_error(name);
}

constexpr double generatedRate = 44100.0;
constexpr int generatedFrames = 110250; // 2.5 seconds, beyond the eight-chunk prime window.

// Original synthetic 440 Hz tone encoded with FFmpeg/Lavc for decoder testing.
// It contains no third-party music or samples.
const char* mp3FixtureBase64 = "SUQzBAAAAAAAIlRTU0UAAAAOAAADTGF2ZjYxLjcuMTAzAAAAAAAAAAAAAAD/+1AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABJbmZvAAAADwAAAAkAAAR8AEVFRUVFRUVFRUVFXFxcXFxcXFxcXFx0dHR0dHR0dHR0dIuLi4uLi4uLi4uLoqKioqKioqKioqK6urq6urq6urq6utHR0dHR0dHR0dHR6Ojo6Ojo6Ojo6Oj//////////////wAAAABMYXZjNjEuMTkAAAAAAAAAAAAAAAAkA8wAAAAAAAAEfGKMhG0AAAAAAAAAAAAAAAAAAAAA//sQZAAAAHkG04UwAAoAAA0goAABBAgzShmhAAAAADSDAAAAEsSzOM4EAEAaEx+7b4eHl5hDwlAVpWAKMNYBhoPEXRthdcaGr/98KA+Ag1wqCv2KAgAG8aW5NGfqgJKy2HaampsnQNf/+xJkCgPwjAbTL2gACAAADSDgAAECRB1WgOEg4AAANIAAAASABBTv5EwBlC+w78bQCNHlaWnQciEGNE8DQwRlpKSQBR2w7APNAF6QKmW5bDCCakGrTSl0hEDmg+5/DsuI0RASWSga8AD/+xBkGoPwiAdQAZswmAAADSAAAAEB2BtdBOEicAAANIAAAATwTAErBiVR2A2RqF0acA0wADKUDseCnl7YgSsAiTGA8eK+AAvEMuoGBwiVcA0wACsSD0dC/165BWUCFFsEYIInmpRYOv/7EmQsg/CHB1ADeTEIAAANIAAAAQIYG1qDYSJgAAA0gAAABI16BsjQCOZlKB1HYxqoAxk/rEKAVEA1wAFH7wAXDMjqB4eIiqtHIqGAUyfnMkBXKN1EKSZRJVgAAO+mciP1O7s/ajADjP/7EGQ+B/CEB1YhGBiYAAANIAAAAQGcG16APGDgAAA0gAAABNbnFBIr/G74KEGLCc/3csQ0/3+OQKA5AMWAslwQlj3B8SneikxBTUUzLjEwMKqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq//sSZFED8IMHViEYGJgAAA0gAAABAfAbQgTtImAAADSAAAAEqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq//sQZGOD8G4G0YEbGJgAAA0gAAABAdgbXIA8wOAAADSAAAAEqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqr/+xJkdwPwfAbQgZswmAAADSAAAAEBuB2B1GAAMAAANIKAAASqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqr/+xBkioABNA5RhmhAAAAADSDAAAAAAAGkHAAAIAAANIOAAASqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqg==";

struct TempFile final {
    explicit TempFile(const juce::String& suffix, const juce::String& stem)
        : file(juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getNonexistentChildFile(stem, suffix, false)) {}
    ~TempFile() { file.deleteFile(); }
    juce::File file;
};

juce::AudioBuffer<float> makeTone() {
    juce::AudioBuffer<float> audio(2, generatedFrames);
    constexpr double twoPi = 6.283185307179586476925286766559;
    for (int i = 0; i < generatedFrames; ++i) {
        const auto phase = twoPi * 440.0 * static_cast<double>(i) / generatedRate;
        audio.setSample(0, i, static_cast<float>(0.35 * std::sin(phase)));
        audio.setSample(1, i, static_cast<float>(0.25 * std::sin(phase + 0.35)));
    }
    return audio;
}

void writeGeneratedFixture(juce::AudioFormat& format, TempFile& target) {
    target.file.deleteFile();
    auto stream = target.file.createOutputStream();
    check(stream != nullptr, "fixture output stream created");
    const auto possibleRates = format.getPossibleSampleRates();
    const double sampleRate = possibleRates.contains(static_cast<int>(generatedRate))
        ? generatedRate : static_cast<double>(possibleRates.getFirst());
    const auto possibleBits = format.getPossibleBitDepths();
    const int bits = possibleBits.contains(16) ? 16 : possibleBits.getFirst();
    check(sampleRate > 0.0 && bits > 0, "fixture format exposes writer settings");

    juce::StringPairArray metadata;
    auto* raw = stream.release();
    const int quality = format.getQualityOptions().isEmpty() ? 0 : format.getQualityOptions().size() / 2;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        format.createWriterFor(raw, sampleRate, 2, bits, metadata, quality));
    if (!writer) { delete raw; throw std::runtime_error("fixture writer creation failed"); }
    auto tone = makeTone();
    check(writer->writeFromAudioSampleBuffer(tone, 0, tone.getNumSamples()), "fixture write succeeds");
    writer.reset();
    check(target.file.existsAsFile() && target.file.getSize() > 0, "generated fixture exists");
}

void writeMp3Fixture(TempFile& target) {
    juce::MemoryOutputStream decoded;
    check(juce::Base64::convertFromBase64(decoded, mp3FixtureBase64), "embedded MP3 base64 decodes");
    check(target.file.replaceWithData(decoded.getData(), decoded.getDataSize()), "embedded MP3 fixture writes");
}

bool anyPeak(const std::vector<float>& peaks) {
    return std::any_of(peaks.begin(), peaks.end(), [](float value) {
        return std::isfinite(value) && value > 0.01f;
    });
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
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + static_cast<double>(timeoutMs);
    while (juce::Time::getMillisecondCounterHiRes() < deadline) {
        if (cache->hasChunk(chunk)) return true;
        juce::Thread::sleep(2);
    }
    return cache->hasChunk(chunk);
}

void exerciseDecode(const juce::File& file, const char* label, bool requireLongSeek) {
    std::atomic<bool> cancelled{false};
    DecodeOptions memoryOptions;
    memoryOptions.streamingThresholdBytes = std::numeric_limits<std::int64_t>::max();
    auto memory = decodeTrack(file, cancelled, memoryOptions);
    check(memory.error.isEmpty() && memory.clip && memory.clip->valid(), "codec decodes in-memory");
    check(!memory.clip->streamed(), "high threshold keeps fixture in memory");
    check(memory.peaks.size() == 512 && anyPeak(memory.peaks), "codec preview contains signal");

    DecodeOptions streamOptions;
    streamOptions.streamingThresholdBytes = 1;
    auto streamed = decodeTrack(file, cancelled, streamOptions);
    check(streamed.error.isEmpty() && streamed.clip && streamed.clip->valid() && streamed.clip->streamed(),
          "codec decodes through streaming path");
    check(streamed.clip->sourceOwner != nullptr, "streaming reader lifetime is retained");
    check(streamed.peaks.size() == 512 && anyPeak(streamed.peaks), "streaming preview contains signal");

    if (requireLongSeek) {
        check(streamed.clip->frames() > static_cast<std::int64_t>(broke::StreamCache::chunkFrames) * 8,
              "long codec fixture exceeds prime window");
        const auto target = streamed.clip->frames() * 3 / 4;
        check(waitForRegion(streamed.clip->stream, target, 2000), "streaming reader refills distant codec seek");
        float sample = 0.0f;
        check(streamed.clip->stream->trySample(0, target, sample) && std::isfinite(sample),
              "refilled codec sample is finite");
    }
    std::cout << "codec OK: " << label << '\n';
}

void runCodecMatrix() {
    juce::WavAudioFormat wav;
    TempFile wavFile(".wav", "BrokeDJ-WAV-fixture");
    writeGeneratedFixture(wav, wavFile);
    exerciseDecode(wavFile.file, "WAV", true);

    juce::AiffAudioFormat aiff;
    TempFile aiffFile(".aiff", juce::String::fromUTF8("BrokeDJ-zażółć-AIFF"));
    writeGeneratedFixture(aiff, aiffFile);
    exerciseDecode(aiffFile.file, "AIFF Unicode path", true);

    juce::FlacAudioFormat flac;
    TempFile flacFile(".flac", "BrokeDJ-FLAC-fixture");
    writeGeneratedFixture(flac, flacFile);
    exerciseDecode(flacFile.file, "FLAC", true);

    juce::OggVorbisAudioFormat ogg;
    TempFile oggFile(".ogg", "BrokeDJ-OGG-fixture");
    writeGeneratedFixture(ogg, oggFile);
    exerciseDecode(oggFile.file, "OGG", true);

    TempFile mp3File(".mp3", "BrokeDJ-MP3-fixture");
    writeMp3Fixture(mp3File);
    exerciseDecode(mp3File.file, "MP3", false);
}

void runReverseDirectionReadAhead() {
    juce::WavAudioFormat wav;
    TempFile wavFile(".wav", "BrokeDJ-reverse-read-ahead-fixture");
    writeGeneratedFixture(wav, wavFile);

    std::atomic<bool> cancelled{false};
    DecodeOptions options;
    options.streamingThresholdBytes = 1;
    auto result = decodeTrack(wavFile.file, cancelled, options);
    check(result.error.isEmpty() && result.clip && result.clip->streamed(),
          "reverse read-ahead fixture uses streaming path");

    auto cache = result.clip->stream;
    constexpr std::int64_t chunk = static_cast<std::int64_t>(broke::StreamCache::chunkFrames);
    const auto forwardTarget = chunk * 22 + 32;
    check(forwardTarget < result.clip->frames(), "forward direction fixture target is inside track");
    check(waitForRegion(cache, forwardTarget, 2000),
          "forward seek establishes a distant read-ahead direction");
    check(waitForChunk(cache, 24, 2000),
          "forward direction fills a deeper chunk ahead of the request");

    const auto reverseTarget = chunk * 15 + 32;
    cache->request(reverseTarget);
    check(waitForRegion(cache, reverseTarget, 2000),
          "backward request becomes the active streamed region");
    check(waitForChunk(cache, 13, 2000),
          "observed backward transport fills deep read-ahead behind the cursor");
}

void runSlowReaderPreemption() {
    juce::WavAudioFormat wav;
    TempFile wavFile(".wav", "BrokeDJ-slow-reader-fixture");
    writeGeneratedFixture(wav, wavFile);

    std::atomic<bool> cancelled{false};
    DecodeOptions options;
    options.streamingThresholdBytes = 1;
    options.readAheadDelayMs = 120;
    auto result = decodeTrack(wavFile.file, cancelled, options);
    check(result.error.isEmpty() && result.clip && result.clip->streamed(), "slow reader uses streaming path");

    auto cache = result.clip->stream;
    const auto targetFrame = result.clip->frames() * 4 / 5;
    cache->request(targetFrame);
    check(waitForRegion(cache, targetFrame, 2000), "new seek preempts stale slow read-ahead window");
    check(!cache->hasChunk(9), "stale forward chunk stays unfetched after seek preemption");

    broke::Engine engine;
    engine.prepare(48000.0);
    check(engine.submit(0, std::move(result.clip)), "slow-reader clip submits");
    std::array<std::array<float, 512>, 4> audio{};
    std::array<float*, 4> outputs{};
    for (std::size_t i = 0; i < outputs.size(); ++i) outputs[i] = audio[i].data();
    engine.process(outputs.data(), 4, 512);
    engine.control(0).playing = true;
    engine.control(0).seek = 0.60;
    engine.process(outputs.data(), 4, 512);
    const auto starved = cache->diagnostics(2);
    check(starved.starving && starved.starvationEvents >= 1, "delayed reader exposes playback starvation");
    check(waitForRegion(cache, starved.requestedFrame, 2000), "delayed reader eventually refills target");
    engine.process(outputs.data(), 4, 512);
    const auto refilled = cache->diagnostics(2);
    check(!refilled.starving && refilled.refillEvents >= 1, "starvation closes after delayed refill");
}

void runFailureChecks() {
    TempFile bad(".wav", "BrokeDJ-invalid-fixture");
    constexpr std::array<unsigned char, 16> garbage {
        0x42, 0x52, 0x4f, 0x4b, 0x45, 0x44, 0x4a, 0x00,
        0xff, 0x7f, 0x13, 0x37, 0x00, 0x01, 0x02, 0x03
    };
    check(bad.file.replaceWithData(garbage.data(), garbage.size()), "invalid fixture writes");
    std::atomic<bool> cancelled{false};
    auto invalid = decodeTrack(bad.file, cancelled);
    check(!invalid.clip && invalid.error.isNotEmpty(), "invalid file fails with diagnostic");

    juce::WavAudioFormat wav;
    TempFile wavFile(".wav", "BrokeDJ-cancel-fixture");
    writeGeneratedFixture(wav, wavFile);
    cancelled = true;
    DecodeOptions forcedStream;
    forcedStream.streamingThresholdBytes = 1;
    auto stopped = decodeTrack(wavFile.file, cancelled, forcedStream);
    check(!stopped.clip && stopped.error.containsIgnoreCase("cancel"), "cancelled import stops publication");
}
}

int main() {
    try {
        runCodecMatrix();
        runReverseDirectionReadAhead();
        runSlowReaderPreemption();
        runFailureChecks();
        std::cout << "PASS: " << checks << " decoder/codec checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}