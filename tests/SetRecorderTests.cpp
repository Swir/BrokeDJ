// SPDX-License-Identifier: AGPL-3.0-only
#include <JuceHeader.h>
#include "app/SetRecorder.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
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
    testCleanFinalize();
    testOverflowIsMeasuredNotBlocking();
    testInvalidStartFailsClosed();
    std::cout << "SetRecorderTests passed\n";
    return 0;
}
