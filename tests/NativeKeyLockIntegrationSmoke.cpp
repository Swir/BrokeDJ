// SPDX-License-Identifier: AGPL-3.0-only
#include <JuceHeader.h>
#include "app/MainComponent.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

int checks = 0;
constexpr double sampleRate = 48000.0;
constexpr int blockFrames = 512;
constexpr double sourceFrequency = 440.0;

void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

struct TempDirectory final {
    TempDirectory()
        : directory(juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getNonexistentChildFile("BrokeDJ-native-keylock-smoke", {}, false)) {
        check(directory.createDirectory().wasOk(), "temporary directory created");
    }

    ~TempDirectory() { directory.deleteRecursively(); }

    juce::File directory;
};

void writeStereoTone(const juce::File& target) {
    constexpr int seconds = 8;
    constexpr int frames = static_cast<int>(sampleRate) * seconds;
    constexpr double twoPi = 6.283185307179586476925286766559;

    juce::AudioBuffer<float> audio(2, frames);
    for (int frame = 0; frame < frames; ++frame) {
        const double time = static_cast<double>(frame) / sampleRate;
        const float value = static_cast<float>(0.24 * std::sin(twoPi * sourceFrequency * time));
        audio.setSample(0, frame, value);
        audio.setSample(1, frame, value * 0.92f);
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
    check(writer->writeFromAudioSampleBuffer(audio, 0, frames), "WAV key-lock fixture written");
    writer.reset();
    check(target.existsAsFile() && target.getSize() > 44, "WAV key-lock fixture exists");
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

DeckPanel* firstDeck(MainComponent& component) {
    for (int index = 0; index < component.getNumChildComponents(); ++index) {
        if (auto* deck = dynamic_cast<DeckPanel*>(component.getChildComponent(index))) return deck;
    }
    return nullptr;
}

juce::TextButton* buttonByText(DeckPanel& deck, const juce::String& label) {
    for (int index = 0; index < deck.getNumChildComponents(); ++index) {
        auto* button = dynamic_cast<juce::TextButton*>(deck.getChildComponent(index));
        if (button != nullptr && button->getButtonText() == label) return button;
    }
    return nullptr;
}

juce::TextButton* playButton(DeckPanel& deck) {
    for (int index = 0; index < deck.getNumChildComponents(); ++index) {
        auto* button = dynamic_cast<juce::TextButton*>(deck.getChildComponent(index));
        if (button != nullptr && (button->getButtonText() == "PLAY" || button->getButtonText() == "PAUSE"))
            return button;
    }
    return nullptr;
}

juce::Slider* rateSlider(DeckPanel& deck) {
    for (int index = 0; index < deck.getNumChildComponents(); ++index) {
        auto* slider = dynamic_cast<juce::Slider*>(deck.getChildComponent(index));
        if (slider == nullptr) continue;
        if (std::abs(slider->getMinimum() + 20.0) < 1.0e-9
            && std::abs(slider->getMaximum() - 20.0) < 1.0e-9) {
            return slider;
        }
    }
    return nullptr;
}

void renderBlocks(MainComponent& component, int blockCount) {
    juce::AudioBuffer<float> output(2, blockFrames);
    juce::AudioSourceChannelInfo info(&output, 0, blockFrames);
    for (int block = 0; block < blockCount; ++block) {
        output.clear();
        component.getNextAudioBlock(info);
    }
}

struct FrequencyMeasurement final {
    double frequencyHz = 0.0;
    double rms = 0.0;
    double peak = 0.0;
};

FrequencyMeasurement renderFrequency(MainComponent& component,
                                     int warmupBlocks = 48,
                                     int measuredBlocks = 64) {
    juce::AudioBuffer<float> output(2, blockFrames);
    juce::AudioSourceChannelInfo info(&output, 0, blockFrames);

    for (int block = 0; block < warmupBlocks; ++block) {
        output.clear();
        component.getNextAudioBlock(info);
    }

    std::vector<float> samples;
    samples.reserve(static_cast<std::size_t>(measuredBlocks * blockFrames));
    double squared = 0.0;
    double peak = 0.0;
    for (int block = 0; block < measuredBlocks; ++block) {
        output.clear();
        component.getNextAudioBlock(info);
        const auto* left = output.getReadPointer(0);
        for (int frame = 0; frame < blockFrames; ++frame) {
            const float value = left[frame];
            check(std::isfinite(value), "native key-lock render stays finite");
            samples.push_back(value);
            squared += static_cast<double>(value) * static_cast<double>(value);
            peak = std::max(peak, std::abs(static_cast<double>(value)));
        }
    }

    check(!samples.empty(), "native key-lock measurement captured samples");
    const double rms = std::sqrt(squared / static_cast<double>(samples.size()));
    check(rms > 0.005, "native key-lock render has measurable signal");
    check(peak < 1.0, "native key-lock render remains bounded");

    const double threshold = std::max(0.0015, peak * 0.08);
    bool armed = false;
    std::vector<std::size_t> crossings;
    crossings.reserve(400);
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const double value = static_cast<double>(samples[index]);
        if (value < -threshold) {
            armed = true;
        } else if (armed && value > threshold) {
            crossings.push_back(index);
            armed = false;
        }
    }

    check(crossings.size() > 100, "native key-lock render exposes stable tone crossings");
    const auto first = crossings.front();
    const auto last = crossings.back();
    check(last > first, "native key-lock tone crossing span is positive");
    const double frequency = static_cast<double>(crossings.size() - 1) * sampleRate
        / static_cast<double>(last - first);
    check(std::isfinite(frequency), "native key-lock frequency estimate is finite");
    return FrequencyMeasurement{frequency, rms, peak};
}

void clickPlay(juce::TextButton& button) {
    check(static_cast<bool>(button.onClick), "native PLAY callback is installed");
    button.onClick();
}

void setToggleAndInvoke(juce::TextButton& button, bool state, const char* message) {
    check(static_cast<bool>(button.onClick), message);
    button.setToggleState(state, juce::dontSendNotification);
    button.onClick();
}

void runNativeKeyLockIntegrationSmoke() {
    TempDirectory temp;
    const auto track = temp.directory.getChildFile("BrokeDJ key-lock integration 440Hz.wav");
    writeStereoTone(track);

    // No physical audio device is opened. Calling prepareToPlay directly gives
    // the real MainComponent/Engine lifecycle a deterministic offline device
    // boundary while keeping the test safe for CI and developer machines.
    MainComponent component(false, true);
    component.prepareToPlay(blockFrames, sampleRate);

    check(component.loadFileIntoDeck(0, track), "native key-lock fixture import accepted");
    check(pumpUntil([&] { return !component.sessionDeckLoading(0); }, 10000),
          "native key-lock fixture import completed within deadline");
    check(component.sessionDeckMatches(0, track), "native key-lock fixture identity published");

    // Adopt the immutable clip at the same callback boundary used by the app.
    renderBlocks(component, 3);
    check(component.sessionDeckDuration(0) > 7.9,
          "native key-lock fixture crossed Engine adoption boundary");

    auto state = component.captureSessionState().decks[0];
    state.playbackRate = 1.20f;
    state.channelGain = 0.90f;
    state.trimDb = 0.0f;
    state.low = 1.0f;
    state.mid = 1.0f;
    state.high = 1.0f;
    state.echo = 0.0f;
    state.drive = 0.0f;
    state.wholeTrackLoop = true;
    state.positionSeconds = 0.0;
    check(component.applyRestoredDeckState(0, state),
          "paused native deck accepted deterministic 1.20x test controls");
    renderBlocks(component, 2); // consume paused seek before staging/play.

    auto* deck = firstDeck(component);
    check(deck != nullptr, "native deck component found");
    auto* play = playButton(*deck);
    auto* rate = rateSlider(*deck);
    auto* loop = buttonByText(*deck, "LOOP");
    auto* cueZero = buttonByText(*deck, "CUE 0");
    check(play != nullptr, "native PLAY control found");
    check(rate != nullptr, "native rate control found");
    check(loop != nullptr, "native whole-track LOOP control found");
    check(cueZero != nullptr, "native CUE 0 control found");
    check(loop->getToggleState(), "native LOOP control mirrors restored whole-track loop");
    check(std::abs(rate->getValue() - 20.0) < 0.02,
          "native rate control mirrors staged 1.20x transport");

    // onBeforePlay invokes the real KeyLockDeckLifecycle service. At 1.20x the
    // research renderer should preserve the 440 Hz fixture instead of following
    // the ordinary pitch-changing converter toward 528 Hz.
    clickPlay(*play);
    check(component.captureSessionState().decks[0].wasPlaying,
          "native deck entered playback through the real PLAY callback");
    const auto locked = renderFrequency(component);
    check(locked.frequencyHz > 425.0 && locked.frequencyHz < 455.0,
          "native key-lock path preserves fixture pitch at 1.20x");

    // A live rate change intentionally disarms the optional renderer. The native
    // slider callback must fail closed to the ordinary production converter and
    // must not perform an off-callback re-prime while transport is running.
    rate->setValue(10.0, juce::sendNotificationSync);
    const auto afterLiveRate = component.captureSessionState();
    check(afterLiveRate.decks[0].wasPlaying,
          "live rate change does not stop ordinary fallback playback");
    check(std::abs(static_cast<double>(afterLiveRate.decks[0].playbackRate) - 1.10) < 0.002,
          "live native rate control updates Engine transport to 1.10x");
    const auto fallback = renderFrequency(component);
    check(fallback.frequencyHz > 468.0 && fallback.frequencyHz < 500.0,
          "live key-lock discontinuity falls back to pitch-changing production playback");
    check(fallback.frequencyHz - locked.frequencyHz > 25.0,
          "native fail-closed render is measurably distinct from key-locked render");

    // Pause is the qualification boundary for deterministic re-prime. The next
    // PLAY must stage the current 1.10x control snapshot and restore pitch lock.
    clickPlay(*play);
    check(!component.captureSessionState().decks[0].wasPlaying,
          "native deck paused before key-lock restage");
    renderBlocks(component, 2);
    clickPlay(*play);
    check(component.captureSessionState().decks[0].wasPlaying,
          "native deck resumed after paused key-lock restage");
    const auto restaged = renderFrequency(component);
    check(restaged.frequencyHz > 425.0 && restaged.frequencyHz < 455.0,
          "paused native restage restores key-locked fixture pitch at 1.10x");
    check(std::abs(restaged.frequencyHz - locked.frequencyHz) < 12.0,
          "native key-lock pitch remains stable across paused rate restage");

    // Whole-track LOOP is another transport snapshot owned by the real DeckPanel.
    // Turning it off live must disarm key lock immediately, keep production
    // playback running at 1.10x and defer research-path restaging until paused.
    setToggleAndInvoke(*loop, false, "native LOOP callback is installed");
    const auto afterLoopChange = component.captureSessionState();
    check(afterLoopChange.decks[0].wasPlaying,
          "live LOOP change preserves ordinary fallback playback");
    check(!afterLoopChange.decks[0].wholeTrackLoop,
          "native LOOP callback updates Engine whole-track loop state");
    const auto loopFallback = renderFrequency(component);
    check(loopFallback.frequencyHz > 468.0 && loopFallback.frequencyHz < 500.0,
          "live LOOP discontinuity falls back to pitch-changing production playback");

    clickPlay(*play);
    check(!component.captureSessionState().decks[0].wasPlaying,
          "native deck paused before LOOP key-lock restage");
    renderBlocks(component, 2);
    clickPlay(*play);
    check(component.captureSessionState().decks[0].wasPlaying,
          "native deck resumed after LOOP key-lock restage");
    const auto loopRestaged = renderFrequency(component);
    check(loopRestaged.frequencyHz > 425.0 && loopRestaged.frequencyHz < 455.0,
          "paused native restage restores key lock after LOOP change");

    // CUE 0 is the native explicit-seek callback. It must stop playback and move
    // the production transport to the start while invalidating the old research
    // snapshot. The next PLAY performs a safe paused restage at that new cursor.
    check(static_cast<bool>(cueZero->onClick), "native CUE 0 callback is installed");
    cueZero->onClick();
    check(!component.captureSessionState().decks[0].wasPlaying,
          "native CUE 0 stops playback before seek restage");
    renderBlocks(component, 3); // consume the production seek while stopped.
    const auto afterCue = component.captureSessionState();
    check(afterCue.decks[0].positionSeconds < 0.05,
          "native CUE 0 moves the adopted deck transport back to track start");
    clickPlay(*play);
    check(component.captureSessionState().decks[0].wasPlaying,
          "native deck resumed after CUE 0 key-lock restage");
    const auto cueRestaged = renderFrequency(component);
    check(cueRestaged.frequencyHz > 425.0 && cueRestaged.frequencyHz < 455.0,
          "native CUE 0 restart restores key-locked source pitch");
    check(std::abs(cueRestaged.frequencyHz - locked.frequencyHz) < 12.0,
          "native key-lock pitch remains stable after explicit CUE 0 seek");

    std::cout << std::fixed << std::setprecision(2)
              << "METRIC native_keylock_locked_hz=" << locked.frequencyHz << '\n'
              << "METRIC native_keylock_live_fallback_hz=" << fallback.frequencyHz << '\n'
              << "METRIC native_keylock_restaged_hz=" << restaged.frequencyHz << '\n'
              << "METRIC native_keylock_loop_fallback_hz=" << loopFallback.frequencyHz << '\n'
              << "METRIC native_keylock_loop_restaged_hz=" << loopRestaged.frequencyHz << '\n'
              << "METRIC native_keylock_cue_restaged_hz=" << cueRestaged.frequencyHz << '\n'
              << "METRIC native_keylock_locked_rms=" << locked.rms << '\n'
              << "METRIC native_keylock_live_fallback_rms=" << fallback.rms << '\n'
              << "METRIC native_keylock_restaged_rms=" << restaged.rms << '\n'
              << "METRIC native_keylock_loop_fallback_rms=" << loopFallback.rms << '\n'
              << "METRIC native_keylock_loop_restaged_rms=" << loopRestaged.rms << '\n'
              << "METRIC native_keylock_cue_restaged_rms=" << cueRestaged.rms << '\n';

    component.releaseResources();
}

} // namespace

int main() {
    try {
        juce::ScopedJuceInitialiser_GUI juceInitialiser;
        runNativeKeyLockIntegrationSmoke();
        std::cout << "BrokeDJ native key-lock integration smoke OK ("
                  << checks << " checks)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "BrokeDJ native key-lock integration smoke FAILED after "
                  << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
