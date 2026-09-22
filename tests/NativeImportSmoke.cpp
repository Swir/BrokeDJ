// SPDX-License-Identifier: AGPL-3.0-only
#include <JuceHeader.h>
#include "app/MainComponent.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <array>
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

void writeStereoWav(const juce::File& target, double leftFrequency, double rightFrequency) {
    constexpr double sampleRate = 48000.0;
    constexpr int frames = 12000;
    constexpr double twoPi = 6.283185307179586476925286766559;

    juce::AudioBuffer<float> audio(2, frames);
    for (int frame = 0; frame < frames; ++frame) {
        const double time = static_cast<double>(frame) / sampleRate;
        audio.setSample(0, frame,
                        static_cast<float>(0.30 * std::sin(twoPi * leftFrequency * time)));
        audio.setSample(1, frame,
                        static_cast<float>(0.22 * std::sin(twoPi * rightFrequency * time + 0.2)));
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
        // JUCE's callAsync() publishes decoder results back to the Windows
        // message thread. Console CTest targets do not run JUCEApplication's
        // normal dispatch loop, so explicitly drain this test process' native
        // queue instead of sleeping until asynchronous imports time out.
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

bool allDecksIdle(const MainComponent& component) noexcept {
    for (std::size_t deck = 0; deck < broke::deckCount; ++deck) {
        if (component.sessionDeckLoading(deck)) return false;
    }
    return true;
}

void runNativeImportSmoke() {
    TempDirectory temp;

    std::array<juce::File, broke::deckCount> tracks;
    for (std::size_t deck = 0; deck < broke::deckCount; ++deck) {
        tracks[deck] = temp.directory.getChildFile(
            "BrokeDJ integrated import deck " + juce::String(static_cast<int>(deck + 1)) + ".wav");
        const double base = 220.0 + static_cast<double>(deck) * 73.0;
        writeStereoWav(tracks[deck], base, base * 1.5);
    }

    const auto replacement = temp.directory.getChildFile("BrokeDJ valid replacement.wav");
    const auto broken = temp.directory.getChildFile("BrokeDJ invalid import fixture.wav");
    const auto missing = temp.directory.getChildFile("BrokeDJ missing import fixture.wav");
    writeStereoWav(replacement, 997.0, 1495.5);
    check(broken.replaceWithText("not a valid audio file\n"), "invalid fixture written");
    check(!missing.existsAsFile(), "missing fixture starts absent");

    MainComponent component(false, false);
    check(!component.loadFileIntoDeck(broke::deckCount, tracks[0]),
          "out-of-range deck rejected");
    check(!component.loadFileIntoDeck(0, missing), "missing file rejected before async decode");

    for (std::size_t deck = 0; deck < broke::deckCount; ++deck) {
        check(component.loadFileIntoDeck(deck, tracks[deck]),
              "parallel native deck import accepted");
    }
    check(!component.loadFileIntoDeck(0, broken),
          "same-deck replacement rejected while initial import is loading");

    check(pumpUntil([&] { return allDecksIdle(component); }, 10000),
          "parallel native imports completed within deadline");
    for (std::size_t deck = 0; deck < broke::deckCount; ++deck) {
        check(component.sessionDeckMatches(deck, tracks[deck]),
              "parallel import published exact source to deck state");
    }

    const auto firstSession = component.captureSessionState();
    for (std::size_t deck = 0; deck < broke::deckCount; ++deck) {
        check(firstSession.decks[deck].path == tracks[deck].getFullPathName().toStdString(),
              "session snapshot captures every imported deck source");
    }

    check(component.loadFileIntoDeck(2, broken), "invalid replacement reaches async decoder");
    check(pumpUntil([&] { return !component.sessionDeckLoading(2); }, 10000),
          "invalid replacement completed within deadline");
    for (std::size_t deck = 0; deck < broke::deckCount; ++deck) {
        check(component.sessionDeckMatches(deck, tracks[deck]),
              "failed replacement preserves all working deck sources");
    }

    const auto afterFailedReplacement = component.captureSessionState();
    for (std::size_t deck = 0; deck < broke::deckCount; ++deck) {
        check(afterFailedReplacement.decks[deck].path == firstSession.decks[deck].path,
              "failed replacement leaves session source identity unchanged");
    }

    check(component.loadFileIntoDeck(1, replacement), "valid replacement accepted");
    check(!component.loadFileIntoDeck(1, broken),
          "same-deck replacement remains serialized during valid replacement");
    check(pumpUntil([&] { return !component.sessionDeckLoading(1); }, 10000),
          "valid replacement completed within deadline");
    check(component.sessionDeckMatches(1, replacement),
          "valid replacement publishes the new source to the target deck");
    for (std::size_t deck = 0; deck < broke::deckCount; ++deck) {
        if (deck == 1) continue;
        check(component.sessionDeckMatches(deck, tracks[deck]),
              "valid replacement does not disturb another deck source");
    }

    const auto finalSession = component.captureSessionState();
    for (std::size_t deck = 0; deck < broke::deckCount; ++deck) {
        const auto& expected = deck == 1 ? replacement : tracks[deck];
        check(finalSession.decks[deck].path == expected.getFullPathName().toStdString(),
              "final session snapshot matches isolated deck replacement state");
    }
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
