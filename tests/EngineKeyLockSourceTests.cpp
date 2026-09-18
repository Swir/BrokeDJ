// SPDX-License-Identifier: AGPL-3.0-only
#include "core/EngineKeyLockSource.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <numbers>
#include <stdexcept>

namespace {
int checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

std::unique_ptr<broke::Clip> makeTone(double sampleRate = 44100.0, int seconds = 20) {
    auto clip = std::make_unique<broke::Clip>();
    clip->sampleRate = sampleRate;
    const auto frames = static_cast<std::size_t>(sampleRate * static_cast<double>(seconds));
    clip->left.resize(frames);
    clip->right.resize(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const double phase = 2.0 * std::numbers::pi * 440.0
            * static_cast<double>(i) / sampleRate;
        clip->left[i] = static_cast<float>(0.14 * std::sin(phase));
        clip->right[i] = static_cast<float>(0.10 * std::sin(phase + 0.17));
    }
    return clip;
}

void run() {
    using Source = broke::EngineKeyLockSource;
    using Snapshot = Source::ControlSnapshot;
    constexpr double sourceRate = 44100.0;
    constexpr double deviceRate = 48000.0;
    constexpr int frames = 512;

    broke::Engine engine;
    engine.prepare(deviceRate, frames);
    Source source;
    check(source.prepare(sourceRate, deviceRate, frames, 4.0),
          "key-lock Engine source prepares outside callback");

    auto clip = makeTone(sourceRate);
    const broke::Clip* clipIdentity = clip.get();
    constexpr Snapshot firstControls{1.25, -2.0f, true};
    check(source.stage(*clipIdentity, 0.0, false, firstControls),
          "exact clip/control snapshot stages outside callback");
    check(engine.setDeckSourceRenderer(0, &source), "key-lock source installs while stopped");
    check(engine.submit(0, std::move(clip)), "staged clip submitted without changing identity");

    std::array<std::array<float, frames>, 4> audio{};
    std::array<float*, 4> outputs{};
    for (std::size_t channel = 0; channel < outputs.size(); ++channel)
        outputs[channel] = audio[channel].data();

    engine.process(outputs.data(), 4, frames); // adopt / paused
    engine.control(0).rate = static_cast<float>(firstControls.playbackRate);
    engine.control(0).gain = 1.0f;
    engine.control(0).headphone = true;
    engine.master = 1.0f;
    engine.crossfader = 0.0f;
    engine.headphoneLevel = 1.0f;
    engine.control(0).playing = true;

    for (int block = 0; block < 40; ++block)
        engine.process(outputs.data(), 4, frames);

    check(source.lastEngineRenderAccepted(), "Engine accepts exact staged key-lock source");
    check(source.lastRenderPath() == Source::RenderPath::stretch,
          "production Engine block selects stretch path after warmup");
    check(source.lastFallbackReason() == Source::FallbackReason::none,
          "qualified production hook has no fallback reason while staged");
    check(engine.meter(0).position.load() > engine.meter(0).audiblePosition.load(),
          "production meter exposes algorithm-latency-compensated audible cursor");
    check(engine.meter(0).peak.load() > 0.001f, "key-lock source still feeds production meter");
    check(std::any_of(audio[2].begin(), audio[2].end(), [](float value) {
        return std::isfinite(value) && std::abs(value) > 0.001f;
    }), "key-lock source preserves production four-output cue routing");

    // A live control target that no longer matches the off-callback stage must
    // fail closed at the adapter boundary. Engine must remain audible by using
    // its built-in converter rather than stale key-lock state.
    engine.control(0).rate = 1.10f;
    engine.process(outputs.data(), 4, frames);
    check(!source.lastEngineRenderAccepted(), "unstaged playback-rate change rejects key-lock source");
    check(std::abs(engine.meter(0).position.load() - engine.meter(0).audiblePosition.load()) < 1.0e-6,
          "unstaged rate change falls back to ordinary production scheduling");
    check(std::any_of(audio[0].begin(), audio[0].end(), [](float value) {
        return std::isfinite(value) && std::abs(value) > 0.001f;
    }), "unstaged rate change remains audible through immediate fallback");

    // Simulate the owner stopping/serializing audio before restaging the exact
    // immutable clip at the Engine-reported transport location.
    const double restageCursor = engine.meter(0).position.load() * sourceRate;
    constexpr Snapshot secondControls{1.10, 1.5f, true};
    check(source.stage(*clipIdentity, restageCursor, false, secondControls),
          "new playback-rate/pitch snapshot restages outside callback");
    for (int block = 0; block < 30; ++block)
        engine.process(outputs.data(), 4, frames);
    check(source.lastEngineRenderAccepted(), "restaged rate returns to key-lock source");
    check(source.lastRenderPath() == Source::RenderPath::stretch,
          "restaged production hook returns to stretch path");

    // Engine seek is intentionally last-request-wins. It moves transport before
    // asking the selector to render; the selector detects the discontinuity and
    // renders its production-style fallback instead of stale stretch history.
    engine.control(0).seek = 0.50;
    engine.process(outputs.data(), 4, frames);
    check(source.lastEngineRenderAccepted(), "seek remains inside safe selector boundary");
    check(source.lastRenderPath() == Source::RenderPath::fallback,
          "seek discontinuity rejects stale stretch history");
    check(source.lastFallbackReason() == Source::FallbackReason::cursorDiscontinuity,
          "seek exposes explicit cursor-discontinuity fallback reason");
    check(std::abs(engine.meter(0).position.load() - engine.meter(0).audiblePosition.load()) < 1.0e-6,
          "seek fallback publishes ordinary audible scheduling");

    // Re-prime at the post-seek transport, then prove loop identity is guarded
    // independently as another discontinuity requiring off-callback restaging.
    const double postSeekCursor = engine.meter(0).position.load() * sourceRate;
    check(source.stage(*clipIdentity, postSeekCursor, false, secondControls),
          "post-seek key-lock state restages explicitly");
    engine.process(outputs.data(), 4, frames);
    check(source.lastRenderPath() == Source::RenderPath::stretch,
          "post-seek restage restores stretch path");

    engine.control(0).loop = true;
    engine.process(outputs.data(), 4, frames);
    check(!source.lastEngineRenderAccepted(), "unstaged loop-mode change rejects adapter at Engine boundary");
    check(std::abs(engine.meter(0).position.load() - engine.meter(0).audiblePosition.load()) < 1.0e-6,
          "loop mismatch uses ordinary production scheduling");

    // Clip replacement must never inherit the previous clip's key-lock state.
    engine.control(0).loop = false;
    check(engine.submit(0, makeTone(sourceRate)), "replacement clip submitted");
    engine.process(outputs.data(), 4, frames); // adopts and pauses
    engine.control(0).rate = 1.10f;
    engine.control(0).playing = true;
    engine.process(outputs.data(), 4, frames);
    check(!source.lastEngineRenderAccepted(), "replacement clip cannot reuse stale key-lock stage");
    check(std::any_of(audio[0].begin(), audio[0].end(), [](float value) {
        return std::isfinite(value) && std::abs(value) > 0.001f;
    }), "replacement clip stays audible through production fallback");

    check(engine.setDeckSourceRenderer(0, nullptr), "key-lock source removes while stopped");
    source.disarm();
}
}

int main() {
    try {
        run();
        std::cout << "PASS: " << checks << " checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
