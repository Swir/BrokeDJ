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
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

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

std::filesystem::path filesystemPath(const juce::File& file) {
#if JUCE_WINDOWS
    return std::filesystem::path(file.getFullPathName().toWideCharPointer());
#else
    return std::filesystem::u8path(file.getFullPathName().toStdString());
#endif
}

bool approximately(double actual, double expected, double tolerance = 1.0e-4) noexcept {
    return std::isfinite(actual) && std::abs(actual - expected) <= tolerance;
}

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

void renderSilentBlocks(MainComponent& component, int blockCount) {
    constexpr int frames = 512;
    juce::AudioBuffer<float> output(4, frames);
    juce::AudioSourceChannelInfo info(&output, 0, frames);
    for (int block = 0; block < blockCount; ++block) {
        output.clear();
        component.getNextAudioBlock(info);
    }
}

void verifyDeckState(const broke::session::DeckState& actual,
                     const broke::session::DeckState& expected,
                     bool expectPath) {
    check(expectPath ? actual.path == expected.path : actual.path.empty(),
          "restored deck source identity matches session");
    check(approximately(actual.playbackRate, expected.playbackRate),
          "restored playback rate matches session");
    check(approximately(actual.trimDb, expected.trimDb),
          "restored trim matches session");
    check(approximately(actual.channelGain, expected.channelGain),
          "restored channel gain matches session");
    check(approximately(actual.low, expected.low), "restored low EQ matches session");
    check(approximately(actual.mid, expected.mid), "restored mid EQ matches session");
    check(approximately(actual.high, expected.high), "restored high EQ matches session");
    check(approximately(actual.echo, expected.echo), "restored echo matches session");
    check(approximately(actual.drive, expected.drive), "restored drive matches session");
    check(actual.headphoneCue == expected.headphoneCue,
          "restored headphone cue matches session");
    check(actual.wholeTrackLoop == expected.wholeTrackLoop,
          "restored whole-track loop matches session");
    check(!actual.wasPlaying, "session restore never auto-resumes playback");
}

void exerciseNativeSessionRoundTrip(MainComponent& component,
                                    const TempDirectory& temp,
                                    const std::array<juce::File, broke::deckCount>& tracks,
                                    const juce::File& replacement) {
    // Adopt the current async imports through the real Engine callback before
    // replacing them with a saved session. This is an offline/no-device render:
    // no physical device is opened and no audible playback is requested.
    component.prepareToPlay(512, 48000.0);
    renderSilentBlocks(component, 3);
    for (std::size_t deck = 0; deck < broke::deckCount; ++deck)
        check(component.sessionDeckDuration(deck) > 0.0,
              "initial imported clip adopted by audio engine");

    broke::session::SessionState wanted = component.captureSessionState();
    wanted.mixer.crossfader = 0.23f;
    wanted.mixer.master = 0.76f;
    wanted.mixer.headphoneLevel = 0.41f;

    const std::array<double, 4> positions{0.035, 0.070, 0.105, 0.0};
    for (std::size_t deck = 0; deck < broke::deckCount; ++deck) {
        auto& state = wanted.decks[deck];
        state.positionSeconds = positions[deck];
        state.playbackRate = static_cast<float>(0.92 + 0.04 * static_cast<double>(deck));
        state.trimDb = static_cast<float>(-3.0 + static_cast<double>(deck));
        state.channelGain = static_cast<float>(0.55 + 0.08 * static_cast<double>(deck));
        state.low = static_cast<float>(0.78 + 0.05 * static_cast<double>(deck));
        state.mid = static_cast<float>(0.88 + 0.04 * static_cast<double>(deck));
        state.high = static_cast<float>(0.98 + 0.03 * static_cast<double>(deck));
        state.echo = static_cast<float>(0.05 * static_cast<double>(deck));
        state.drive = static_cast<float>(0.40 * static_cast<double>(deck));
        state.headphoneCue = (deck % 2) == 0;
        state.wholeTrackLoop = (deck % 2) != 0;
        state.wasPlaying = true; // persisted history must not auto-resume on restore.
    }
    wanted.decks[0].path = tracks[0].getFullPathName().toStdString();
    wanted.decks[1].path = replacement.getFullPathName().toStdString();
    wanted.decks[2].path = tracks[2].getFullPathName().toStdString();
    wanted.decks[3].path.clear(); // Exercise exact empty-slot ejection during restore.

    broke::session::SessionStore store;
    const auto sessionFile = temp.directory.getChildFile("native-session-roundtrip.brokedj-session");
    std::string error;
    check(store.save(filesystemPath(sessionFile), wanted, &error),
          "native session snapshot saved atomically");
    check(error.empty(), "session save reports no error");
    auto loaded = store.load(filesystemPath(sessionFile), &error);
    check(loaded.has_value(), "saved native session reloads with checksum validation");
    check(error.empty(), "session load reports no error");

    // Also cover explicit verified .bak recovery without changing production
    // behavior: ordinary load must reject the damaged primary, while the opt-in
    // recovery API may use a byte-identical verified backup.
    const auto recoveryFile = temp.directory.getChildFile("native-session-recovery.brokedj-session");
    error.clear();
    check(store.save(filesystemPath(recoveryFile), wanted, &error),
          "recovery session snapshot saved");
    const juce::File recoveryBackup(recoveryFile.getFullPathName() + ".bak");
    check(recoveryFile.copyFileTo(recoveryBackup), "verified recovery backup fixture copied");
    check(recoveryFile.replaceWithText("damaged primary session\n"),
          "primary recovery fixture deliberately corrupted");
    error.clear();
    check(!store.load(filesystemPath(recoveryFile), &error).has_value(),
          "ordinary session load rejects corrupted primary");
    bool usedBackup = false;
    error.clear();
    const auto recovered = store.loadRecoveringBackup(
        filesystemPath(recoveryFile), &usedBackup, &error);
    check(recovered.has_value() && usedBackup,
          "explicit recovery loads verified backup after primary corruption");
    check(recovered->decks[1].path == wanted.decks[1].path,
          "recovered backup preserves deck source identity");

    check(loaded.has_value(), "roundtrip state remains available for native restore");
    component.prepareForSessionRestore(loaded->mixer);

    for (std::size_t deck = 0; deck < 3; ++deck) {
        const juce::File source(juce::String::fromUTF8(loaded->decks[deck].path.c_str()));
        check(component.loadFileIntoDeck(deck, source),
              "session restore async deck import accepted");
    }
    check(component.beginSessionDeckEject(3),
          "session restore empty slot eject accepted");
    component.armSessionDeckAdoptionMarker(3);

    check(pumpUntil([&] { return allDecksIdle(component); }, 10000),
          "session restore async imports completed within deadline");
    for (std::size_t deck = 0; deck < 3; ++deck) {
        const juce::File source(juce::String::fromUTF8(loaded->decks[deck].path.c_str()));
        check(component.sessionDeckMatches(deck, source),
              "session restore published expected source before adoption");
        component.armSessionDeckAdoptionMarker(deck);
    }

    renderSilentBlocks(component, 3);
    for (std::size_t deck = 0; deck < 3; ++deck) {
        check(component.sessionDeckAdoptionMarkerConsumed(deck),
              "session restore source crossed audio-engine adoption boundary");
        check(component.sessionDeckDuration(deck) > 0.0,
              "adopted session source exposes positive duration");
        check(component.applyRestoredDeckState(deck, loaded->decks[deck]),
              "saved deck controls applied only after source adoption");
    }
    check(component.sessionDeckDuration(3) <= 0.0,
          "empty session slot clears previous audio-owned source");
    check(component.completeSessionDeckEject(3, loaded->decks[3]),
          "empty session slot finalizes only after engine clear");

    // Process the restored seek mailboxes while transport is paused, then verify
    // the public session snapshot observes the same state that the real workflow
    // would expose after its timer-driven restore completes.
    renderSilentBlocks(component, 2);
    const auto restored = component.captureSessionState();
    check(approximately(restored.mixer.crossfader, loaded->mixer.crossfader),
          "restored crossfader matches session");
    check(approximately(restored.mixer.master, loaded->mixer.master),
          "restored master level matches session");
    check(approximately(restored.mixer.headphoneLevel, loaded->mixer.headphoneLevel),
          "restored headphone level matches session");

    for (std::size_t deck = 0; deck < broke::deckCount; ++deck)
        verifyDeckState(restored.decks[deck], loaded->decks[deck], deck < 3);

    for (std::size_t deck = 0; deck < 3; ++deck) {
        check(approximately(restored.decks[deck].positionSeconds,
                            loaded->decks[deck].positionSeconds, 0.012),
              "restored paused deck position matches saved session");
    }
    check(approximately(restored.decks[3].positionSeconds, 0.0, 1.0e-6),
          "restored empty deck position is zero");

    component.releaseResources();
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

    exerciseNativeSessionRoundTrip(component, temp, tracks, replacement);
}

} // namespace

int main() {
    try {
        juce::ScopedJuceInitialiser_GUI juceInitialiser;
        runNativeImportSmoke();
        std::cout << "BrokeDJ native import/session smoke OK (" << checks << " checks)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "BrokeDJ native import/session smoke FAILED after " << checks
                  << " checks: " << error.what() << '\n';
        return 1;
    }
}
