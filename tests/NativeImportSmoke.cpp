// SPDX-License-Identifier: AGPL-3.0-only
#include <JuceHeader.h>
#include "app/MainComponent.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {

int checks = 0;

void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

struct TempDirectory final {
    TempDirectory()
        : directory(juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getNonexistentChildFile("BrokeDJ-native-import-smoke", {}, false)) {
        check(directory.createDirectory().wasOk(), "temporary directory created");
    }

    ~TempDirectory() { directory.deleteRecursively(); }

    juce::File directory;
};

void writeStereoWav(const juce::File& target) {
    constexpr double sampleRate = 48000.0;
    constexpr int frames = 12000;
    constexpr double twoPi = 6.283185307179586476925286766559;

    juce::AudioBuffer<float> audio(2, frames);
    for (int frame = 0; frame < frames; ++frame) {
        const double time = static_cast<double>(frame) / sampleRate;
        audio.setSample(0, frame, static_cast<float>(0.30 * std::sin(twoPi * 440.0 * time)));
        audio.setSample(1, frame, static_cast<float>(0.22 * std::sin(twoPi * 660.0 * time + 0.2)));
    }

    auto stream = target.createOutputStream();
    check(stream != nullptr, "WAV output stream created");
    juce::WavAudioFormat wav;
    juce::StringPairArray metadata;
    auto* raw = stream.release();
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(raw, sampleRate, 2, 16, metadata, 0));
    if (!writer) {
        delete raw;
        throw std::runtime_error("WAV writer created");
    }
    check(writer->writeFromAudioSampleBuffer(audio, 0, frames), "WAV fixture written");
    writer.reset();
    check(target.existsAsFile() && target.getSize() > 44, "WAV fixture exists");
}

#if defined(_WIN32)
bool dispatchNativeMessages() noexcept {
    MSG message{};
    while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) return false;
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }
    return true;
}
#endif

bool pumpUntil(const std::function<bool()>& predicate, int timeoutMs) {
    auto* messages = juce::MessageManager::getInstance();
    if (messages == nullptr) return false;

    const double deadline = juce::Time::getMillisecondCounterHiRes()
        + static_cast<double>(timeoutMs);
    while (juce::Time::getMillisecondCounterHiRes() < deadline) {
        if (predicate()) return true;
#if defined(_WIN32)
        // JUCE's callAsync() publishes the decoder result back to the Windows
        // message thread. Console CTest targets do not run JUCEApplication's
        // normal dispatch loop, so explicitly drain this test process' native
        // queue instead of sleeping until the asynchronous import times out.
        if (!dispatchNativeMessages()) return predicate();
        juce::Thread::sleep(1);
#elif JUCE_MODAL_LOOPS_PERMITTED
        if (!messages->runDispatchLoopUntil(10)) return predicate();
#else
        juce::Thread::sleep(10);
#endif
    }
#if defined(_WIN32)
    static_cast<void>(dispatchNativeMessages());
#endif
    return predicate();
}

void runNativeImportSmoke() {
    TempDirectory temp;
    const auto good = temp.directory.getChildFile("BrokeDJ integrated import fixture.wav");
    const auto broken = temp.directory.getChildFile("BrokeDJ invalid import fixture.wav");
    writeStereoWav(good);
    check(broken.replaceWithText("not a valid audio file\n"), "invalid fixture written");

    MainComponent component(false, false);
    check(!component.loadFileIntoDeck(broke::deckCount, good), "out-of-range deck rejected");
    check(component.loadFileIntoDeck(0, good), "valid native import accepted");
    check(!component.loadFileIntoDeck(0, broken), "concurrent import rejected while deck is loading");

    check(pumpUntil([&] { return !component.sessionDeckLoading(0); }, 10000),
          "valid native import completed within deadline");
    check(component.sessionDeckMatches(0, good), "valid import published exact source to deck state");

    const auto firstSession = component.captureSessionState();
    check(firstSession.decks[0].path == good.getFullPathName().toStdString(),
          "session snapshot captures imported source");
    for (std::size_t deck = 1; deck < broke::deckCount; ++deck)
        check(firstSession.decks[deck].path.empty(), "unused deck remains empty");

    check(component.loadFileIntoDeck(0, broken), "invalid replacement reaches async decoder");
    check(pumpUntil([&] { return !component.sessionDeckLoading(0); }, 10000),
          "invalid replacement completed within deadline");
    check(component.sessionDeckMatches(0, good),
          "failed replacement preserves previous working deck source");

    const auto secondSession = component.captureSessionState();
    check(secondSession.decks[0].path == firstSession.decks[0].path,
          "failed replacement preserves session source identity");
}

} // namespace

int main() {
    try {
        juce::ScopedJuceInitialiser_GUI juceInitialiser;
        runNativeImportSmoke();
        std::cout << "BrokeDJ native import smoke OK (" << checks << " checks)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "BrokeDJ native import smoke FAILED after " << checks
                  << " checks: " << error.what() << '\n';
        return 1;
    }
}
