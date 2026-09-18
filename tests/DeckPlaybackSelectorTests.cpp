// SPDX-License-Identifier: AGPL-3.0-only
#include "core/DeckPlaybackSelector.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace {
int checks = 0;

void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

broke::Clip makeTone(double sampleRate, double frequency, int seconds = 20) {
    broke::Clip clip;
    clip.sampleRate = sampleRate;
    const auto frames = static_cast<std::size_t>(sampleRate * static_cast<double>(seconds));
    clip.left.resize(frames);
    clip.right.resize(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const double phase = 2.0 * std::numbers::pi * frequency
            * static_cast<double>(i) / sampleRate;
        const float value = static_cast<float>(0.12 * std::sin(phase));
        clip.left[i] = value;
        clip.right[i] = value * 0.75f;
    }
    return clip;
}

void runPathSelectionAndMeterSemantics() {
    using Selector = broke::DeckPlaybackSelector;
    using Snapshot = Selector::ControlSnapshot;
    using Path = Selector::RenderPath;
    using Reason = Selector::FallbackReason;

    constexpr double sourceRate = 44100.0;
    constexpr double deviceRate = 48000.0;
    constexpr int frames = 1024;
    auto clip = makeTone(sourceRate, 440.0);

    Selector selector;
    check(selector.prepare(sourceRate, deviceRate, 4096, 4.0),
          "deck selector prepares outside callback");
    check(selector.prepared(), "deck selector reports prepared state");

    double cursor = sourceRate * 2.0 + 0.5;
    constexpr Snapshot bypass{1.0, 0.0f, false};
    check(selector.stage(clip, cursor, false, bypass),
          "bypass snapshot stages transactionally");
    check(!selector.armed(), "bypass snapshot does not arm key lock");

    std::vector<float> left(frames);
    std::vector<float> right(frames);
    double next = cursor;
    double audible = cursor;
    check(selector.render(clip, cursor, false, left.data(), right.data(), frames, next, audible),
          "bypass render succeeds");
    check(selector.lastRenderPath() == Path::fallback,
          "bypass selects production fallback");
    check(selector.lastFallbackReason() == Reason::disabled,
          "bypass reason remains diagnosable");
    check(std::abs(next - audible) < 1.0e-9,
          "fallback transport and audible cursors match");
    check(std::abs(selector.transportCursor() - next) < 1.0e-9
              && std::abs(selector.audibleCursor() - audible) < 1.0e-9,
          "selector publishes fallback meter cursors");
    const float fallbackTailLeft = left.back();
    cursor = next;

    constexpr Snapshot keyLock{1.25, 0.0f, true};
    check(selector.stage(clip, cursor, false, keyLock),
          "key-lock snapshot primes outside callback");
    check(selector.armed() && !selector.needsStage(),
          "key-lock stage is armed without prime debt");
    check(selector.render(clip, cursor, false, left.data(), right.data(), frames, next, audible),
          "key-lock render succeeds");
    check(selector.lastRenderPath() == Path::stretch,
          "primed selector chooses key-lock path");
    check(selector.lastFallbackReason() == Reason::none,
          "key-lock path reports no fallback reason");
    check(audible < next,
          "key-lock audible cursor includes algorithm-latency compensation");
    check(std::abs(left.front() - fallbackTailLeft) < 0.25f,
          "fallback-to-key-lock transition starts from previous rendered tail");
    check(selector.transitionFramesRemaining() == 0,
          "five-millisecond path transition completes inside 1024-frame block");
    cursor = next;

    selector.disarm();
    check(!selector.armed(), "explicit disarm clears key-lock intent");
    const float stretchTailLeft = left.back();
    check(selector.render(clip, cursor, false, left.data(), right.data(), frames, next, audible),
          "disarmed selector renders immediate fallback");
    check(selector.lastRenderPath() == Path::fallback,
          "disarm returns to fallback path");
    check(std::abs(next - audible) < 1.0e-9,
          "disarmed fallback does not retain algorithm latency");
    check(std::abs(left.front() - stretchTailLeft) < 0.25f,
          "key-lock-to-fallback transition starts from previous rendered tail");
}

void runDiscontinuityAndFailClosedMatrix() {
    using Selector = broke::DeckPlaybackSelector;
    using Snapshot = Selector::ControlSnapshot;
    using Path = Selector::RenderPath;
    using Reason = Selector::FallbackReason;

    constexpr double sampleRate = 48000.0;
    constexpr int frames = 512;
    auto clip = makeTone(sampleRate, 330.0);
    auto replacement = makeTone(sampleRate, 550.0);

    Selector selector;
    check(selector.prepare(sampleRate, sampleRate, 2048, 4.0),
          "matrix selector prepares");

    double cursor = 6000.25;
    Snapshot staged{1.10, 2.0f, true};
    check(selector.stage(clip, cursor, false, staged),
          "matrix key-lock snapshot stages");

    std::vector<float> left(frames);
    std::vector<float> right(frames);
    double next = cursor;
    double audible = cursor;
    check(selector.render(clip, cursor, false, left.data(), right.data(), frames, next, audible),
          "matrix primed block renders");
    check(selector.lastRenderPath() == Path::stretch,
          "matrix starts on stretch path");
    cursor = next;

    const double jumped = cursor + 2000.0;
    check(selector.render(clip, jumped, false, left.data(), right.data(), frames, next, audible),
          "unstaged seek fails closed without callback re-prime");
    check(selector.lastRenderPath() == Path::fallback
              && selector.lastFallbackReason() == Reason::cursorDiscontinuity,
          "unstaged seek selects diagnosable fallback");
    check(selector.needsStage(), "seek records off-callback restage debt");

    check(selector.stage(clip, jumped, false, staged),
          "seek restages exact transport outside callback");
    check(selector.render(clip, jumped, false, left.data(), right.data(), frames, next, audible),
          "restaged seek resumes key-lock path");
    check(selector.lastRenderPath() == Path::stretch,
          "restaged seek chooses stretch");
    cursor = next;

    check(selector.render(replacement, cursor, false,
                          left.data(), right.data(), frames, next, audible),
          "clip replacement fails closed");
    check(selector.lastRenderPath() == Path::fallback
              && selector.lastFallbackReason() == Reason::clipChanged,
          "clip replacement is diagnosable");

    Snapshot invalid{std::numeric_limits<double>::quiet_NaN(), 0.0f, true};
    check(!selector.stage(clip, cursor, false, invalid),
          "invalid staged controls fail closed");
    check(!selector.armed(), "invalid stage disarms key-lock intent");
    check(selector.render(clip, cursor, false, left.data(), right.data(), frames, next, audible),
          "invalid stage leaves fallback render available");
    check(selector.lastRenderPath() == Path::fallback,
          "invalid stage cannot select stretch path");

    staged = Snapshot{0.80, -3.0f, true};
    check(selector.stage(clip, cursor, true, staged),
          "loop-enabled snapshot stages independently");
    check(selector.render(clip, cursor, true, left.data(), right.data(), frames, next, audible),
          "loop-enabled staged selector renders");
    check(selector.lastRenderPath() == Path::stretch,
          "loop-enabled exact identity selects stretch");
    check(std::all_of(left.begin(), left.end(), [](float value) { return std::isfinite(value); })
              && std::all_of(right.begin(), right.end(), [](float value) { return std::isfinite(value); }),
          "selector output remains finite across transition matrix");
}
} // namespace

int main() {
    try {
        runPathSelectionAndMeterSemantics();
        runDiscontinuityAndFailClosedMatrix();
        std::cout << "PASS: " << checks << " deck playback selector checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
