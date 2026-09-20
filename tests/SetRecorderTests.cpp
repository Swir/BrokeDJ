// SPDX-License-Identifier: AGPL-3.0-only
#include <juce_audio_formats/juce_audio_formats.h>
#include "app/SetRecorder.h"
#include "core/MasterPathProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace {
[[noreturn]] void fail(const char* message) {
    std::cerr << "SetRecorderTests: " << message << '\n';
    std::exit(1);
}

void require(bool condition, const char* message) {
    if (!condition) fail(message);
}

juce::File freshDirectory(const juce::String& suffix) {
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("BrokeDJ-SetRecorderTests-" + suffix + "-" + juce::Uuid().toString());
    require(dir.createDirectory(), "could not create temporary test directory");
    return dir;
}

void verifyWav(const juce::File& file, double expectedRate, std::int64_t expectedFrames) {
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    require(reader != nullptr, "finalized WAV could not be opened");
    require(reader->numChannels == 2, "recording is not stereo");
    require(std::abs(reader->sampleRate - expectedRate) < 0.5, "recording sample rate mismatch");
    require(reader->lengthInSamples == expectedFrames, "recording frame count mismatch");

    const int inspect = static_cast<int>(std::min<std::int64_t>(reader->lengthInSamples, 512));
    juce::AudioBuffer<float> buffer(2, std::max(1, inspect));
    require(reader->read(&buffer, 0, inspect, 0, true, true), "recorded WAV could not be read");
    for (int channel = 0; channel < 2; ++channel)
        for (int frame = 0; frame < inspect; ++frame)
            require(std::isfinite(buffer.getSample(channel, frame)), "recorded WAV contains non-finite samples");
}

void testMasterPathTransparentBelowCeiling() {
    constexpr int frames = 1024;
    broke::MasterPathProcessor processor;
    processor.prepare(48000.0);
    processor.setMicrophoneEnabled(false);
    processor.setLimiterEnabled(true);

    std::vector<float> left(frames, 0.25f), right(frames, -0.20f);
    processor.process(nullptr, left.data(), right.data(), frames);

    for (int i = 0; i < frames; ++i) {
        require(std::abs(left[static_cast<std::size_t>(i)] - 0.25f) < 1.0e-6f,
                "master limiter changed below-ceiling left audio");
        require(std::abs(right[static_cast<std::size_t>(i)] + 0.20f) < 1.0e-6f,
                "master limiter changed below-ceiling right audio");
    }
    const auto state = processor.snapshot();
    require(state.maxGainReductionDb < 1.0e-4f,
            "below-ceiling master path reported false limiter reduction");
    require(state.maxOutputPeak > 0.249f && state.maxOutputPeak < 0.251f,
            "master-path peak evidence does not match transparent fixture");
}

void testMasterLimiterBoundsAndLinksStereo() {
    constexpr int frames = 2048;
    broke::MasterPathProcessor processor;
    processor.prepare(48000.0);
    processor.setMicrophoneEnabled(false);
    processor.setLimiterEnabled(true);
    processor.setLimiterCeilingDb(-1.0f);

    std::vector<float> left(frames, 1.50f), right(frames, 0.75f);
    processor.process(nullptr, left.data(), right.data(), frames);

    const float ceiling = std::pow(10.0f, -1.0f / 20.0f);
    float peak = 0.0f;
    for (int i = 0; i < frames; ++i) {
        const auto l = left[static_cast<std::size_t>(i)];
        const auto r = right[static_cast<std::size_t>(i)];
        require(std::isfinite(l) && std::isfinite(r), "master limiter produced non-finite output");
        peak = std::max(peak, std::max(std::abs(l), std::abs(r)));
        require(std::abs(r) > 1.0e-6f && std::abs(l / r - 2.0f) < 1.0e-4f,
                "linked-stereo limiter changed the channel ratio");
    }
    const auto state = processor.snapshot();
    require(peak <= ceiling + 1.0e-5f, "sample-peak limiter exceeded its configured ceiling");
    require(state.maxInputPeak > 1.49f, "limiter did not retain pre-reduction peak evidence");
    require(state.maxGainReductionDb > 4.0f, "limiter did not report expected gain reduction");
    require(state.limiterGain < 0.70f, "limiter gain state did not remain reduced under sustained overload");
}

void testMicrophoneDuckingMixesWithoutBlockingMaster() {
    constexpr int frames = 4096;
    broke::MasterPathProcessor processor;
    processor.prepare(48000.0);
    processor.setLimiterEnabled(false);
    processor.setMicrophoneEnabled(true);
    processor.setMicrophoneGainDb(0.0f);
    processor.setDuckDepthDb(12.0f);

    std::vector<float> left(frames, 0.50f), right(frames, 0.50f), microphone(frames, 0.20f);
    processor.process(microphone.data(), left.data(), right.data(), frames);

    const auto state = processor.snapshot();
    require(state.microphoneEnabled, "microphone fixture was not enabled");
    require(state.microphoneEnvelope > 0.15f, "microphone envelope did not follow sustained speech-level input");
    require(state.duckGain < 0.40f, "microphone activity did not materially duck the music bus");
    require(left.back() > 0.20f && left.back() < 0.45f,
            "microphone mix/duck output is outside the expected bounded range");
    require(std::abs(left.back() - right.back()) < 1.0e-6f,
            "mono microphone injection unexpectedly unbalanced the stereo master");
}

void testMasterPathSanitizesNonFiniteInput() {
    broke::MasterPathProcessor processor;
    processor.prepare(48000.0);
    processor.setMicrophoneEnabled(true);
    processor.setLimiterEnabled(true);

    std::vector<float> left{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        0.5f};
    std::vector<float> right{
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN(),
        0.25f,
        -std::numeric_limits<float>::infinity()};
    std::vector<float> microphone(left.size(), std::numeric_limits<float>::quiet_NaN());
    processor.process(microphone.data(), left.data(), right.data(), static_cast<int>(left.size()));

    require(std::all_of(left.begin(), left.end(), [](float value) { return std::isfinite(value); }),
            "master-path sanitation left a non-finite left sample");
    require(std::all_of(right.begin(), right.end(), [](float value) { return std::isfinite(value); }),
            "master-path sanitation left a non-finite right sample");
    const auto state = processor.snapshot();
    require(std::isfinite(state.maxInputPeak) && std::isfinite(state.maxOutputPeak)
                && std::isfinite(state.maxGainReductionDb),
            "master-path metrics became non-finite after invalid input");
}

void testBoothRoutingUsesProtectedMasterAndIndependentLevel() {
    constexpr int frames = 2048;
    broke::MasterPathProcessor processor;
    processor.prepare(48000.0);
    processor.setLimiterEnabled(true);
    processor.setLimiterCeilingDb(-1.0f);
    processor.setBoothEnabled(true);
    processor.setBoothGainDb(-6.0f);

    std::vector<float> left(frames, 1.50f), right(frames, 0.75f);
    std::vector<float> boothLeft(frames, 9.0f), boothRight(frames, 9.0f);
    processor.process(nullptr, left.data(), right.data(), frames,
                      boothLeft.data(), boothRight.data());

    const float boothGain = std::pow(10.0f, -6.0f / 20.0f);
    const float ceiling = std::pow(10.0f, -1.0f / 20.0f);
    for (int i = 0; i < frames; ++i) {
        const auto index = static_cast<std::size_t>(i);
        require(std::abs(left[index]) <= ceiling + 1.0e-5f,
                "booth fixture master exceeded limiter ceiling");
        require(std::abs(boothLeft[index] - left[index] * boothGain) < 2.0e-5f,
                "booth left is not a post-limiter copy at the configured level");
        require(std::abs(boothRight[index] - right[index] * boothGain) < 2.0e-5f,
                "booth right is not a post-limiter copy at the configured level");
    }
    const auto state = processor.snapshot();
    require(state.boothEnabled, "booth routing state did not remain enabled");
    require(std::abs(state.boothGain - boothGain) < 1.0e-5f,
            "booth gain state does not match the configured attenuation");
    require(state.maxBoothPeak > 0.44f && state.maxBoothPeak < 0.45f,
            "booth peak evidence does not match the protected -6 dB route");
}

void testBoothDisabledClearsDedicatedOutputs() {
    constexpr int frames = 256;
    broke::MasterPathProcessor processor;
    processor.prepare(48000.0);
    processor.setLimiterEnabled(false);
    processor.setBoothEnabled(false);

    std::vector<float> left(frames, 0.25f), right(frames, -0.25f);
    std::vector<float> boothLeft(frames, 0.75f), boothRight(frames, -0.75f);
    processor.process(nullptr, left.data(), right.data(), frames,
                      boothLeft.data(), boothRight.data());

    require(std::all_of(boothLeft.begin(), boothLeft.end(), [](float value) { return value == 0.0f; })
                && std::all_of(boothRight.begin(), boothRight.end(), [](float value) { return value == 0.0f; }),
            "disabled booth route leaked stale samples to dedicated outputs");
    require(std::abs(left.back() - 0.25f) < 1.0e-6f && std::abs(right.back() + 0.25f) < 1.0e-6f,
            "disabling booth unexpectedly changed the master path");
    require(processor.snapshot().maxBoothPeak == 0.0f,
            "disabled booth route reported false output peak evidence");
}

void testCleanFinalize() {
    constexpr double rate = 48000.0;
    constexpr int frames = 2048;
    auto dir = freshDirectory("clean");
    const auto requested = dir.getChildFile("set.wav");

    std::vector<float> left(frames), right(frames);
    for (int i = 0; i < frames; ++i) {
        const auto phase = static_cast<double>(i) / rate;
        left[static_cast<std::size_t>(i)] = static_cast<float>(0.3 * std::sin(2.0 * juce::MathConstants<double>::pi * 440.0 * phase));
        right[static_cast<std::size_t>(i)] = static_cast<float>(0.25 * std::sin(2.0 * juce::MathConstants<double>::pi * 660.0 * phase));
    }

    SetRecorder recorder(4096);
    require(recorder.start(requested, rate), "clean recording failed to start");
    recorder.capture(left.data(), right.data(), frames);
    recorder.stop();
    const auto state = recorder.snapshot();
    require(!state.recording, "recorder remained armed after stop");
    require(state.finalized, "clean recording did not finalize");
    require(state.dropoutEvents == 0 && state.droppedFrames == 0, "clean recording reported a false dropout");
    require(state.writtenFrames == frames, "clean recording did not write every frame");
    require(state.destination.existsAsFile(), "clean recording final file missing");
    require(!state.recoveryFile.existsAsFile(), "clean recording left a .part recovery file");
    verifyWav(state.destination, rate, static_cast<std::int64_t>(state.writtenFrames));
    require(dir.deleteRecursively(), "could not remove clean recording fixture");
}

void testOverflowIsMeasuredNotBlocking() {
    constexpr double rate = 44100.0;
    constexpr int frames = 8192;
    auto dir = freshDirectory("overflow");
    const auto requested = dir.getChildFile("overflow.wav");
    std::vector<float> left(frames, 0.2f), right(frames, -0.2f);

    SetRecorder recorder(4096);
    require(recorder.start(requested, rate), "overflow recording failed to start");
    // One callback-sized submission is deliberately larger than the entire FIFO.
    // prepareToWrite can therefore accept at most 4096 frames regardless of writer
    // scheduling, making dropout evidence deterministic without sleeping.
    recorder.capture(left.data(), right.data(), frames);
    recorder.stop();
    const auto state = recorder.snapshot();
    require(state.finalized, "overflow fixture did not preserve a valid finalized WAV");
    require(state.dropoutEvents >= 1, "overflow was not counted as a dropout event");
    require(state.droppedFrames >= 4096, "overflow did not account for the unavailable FIFO tail");
    require(state.writtenFrames > 0 && state.writtenFrames <= 4096, "overflow wrote an impossible frame count");
    verifyWav(state.destination, rate, static_cast<std::int64_t>(state.writtenFrames));
    require(dir.deleteRecursively(), "could not remove overflow recording fixture");
}

void testLateDestinationIsNeverOverwritten() {
    constexpr double rate = 48000.0;
    constexpr int frames = 512;
    auto dir = freshDirectory("collision");
    const auto requested = dir.getChildFile("collision.wav");
    std::vector<float> left(frames, 0.1f), right(frames, -0.1f);

    SetRecorder recorder(4096);
    require(recorder.start(requested, rate), "collision fixture failed to start");
    recorder.capture(left.data(), right.data(), frames);
    // Keep the collision marker free of line endings so this assertion proves
    // overwrite protection rather than platform-specific text normalization.
    require(requested.replaceWithText("external-owner"), "could not create late destination collision");
    recorder.stop();
    const auto state = recorder.snapshot();
    require(!state.finalized, "late destination collision was incorrectly finalized");
    require(requested.loadFileAsString() == "external-owner", "late destination was overwritten");
    require(state.recoveryFile.existsAsFile(), "collision did not retain recovery recording");
    require(state.error.containsIgnoreCase("not overwritten"), "collision did not expose overwrite protection error");
    require(dir.deleteRecursively(), "could not remove collision recording fixture");
}

void testInvalidStartFailsClosed() {
    auto dir = freshDirectory("invalid");
    SetRecorder recorder;
    require(!recorder.start(dir.getChildFile("invalid.wav"), 1000.0), "invalid sample rate was accepted");
    const auto state = recorder.snapshot();
    require(!state.recording && !state.finalized, "invalid start changed recorder lifecycle state");
    require(state.error.isNotEmpty(), "invalid start did not expose an error");
    require(dir.deleteRecursively(), "could not remove invalid recording fixture");
}
} // namespace

int main() {
    testMasterPathTransparentBelowCeiling();
    testMasterLimiterBoundsAndLinksStereo();
    testMicrophoneDuckingMixesWithoutBlockingMaster();
    testMasterPathSanitizesNonFiniteInput();
    testBoothRoutingUsesProtectedMasterAndIndependentLevel();
    testBoothDisabledClearsDedicatedOutputs();
    testCleanFinalize();
    testOverflowIsMeasuredNotBlocking();
    testLateDestinationIsNeverOverwritten();
    testInvalidStartFailsClosed();
    std::cout << "SetRecorderTests passed\n";
    return 0;
}
