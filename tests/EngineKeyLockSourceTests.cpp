// SPDX-License-Identifier: AGPL-3.0-only
#include "core/EngineKeyLockDeckOwner.h"
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

void runSourceBoundary() {
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

    engine.control(0).rate = 1.10f;
    engine.process(outputs.data(), 4, frames);
    check(!source.lastEngineRenderAccepted(), "unstaged playback-rate change rejects key-lock source");
    check(std::abs(engine.meter(0).position.load() - engine.meter(0).audiblePosition.load()) < 1.0e-6,
          "unstaged rate change falls back to ordinary production scheduling");
    check(std::any_of(audio[0].begin(), audio[0].end(), [](float value) {
        return std::isfinite(value) && std::abs(value) > 0.001f;
    }), "unstaged rate change remains audible through immediate fallback");

    const double restageCursor = engine.meter(0).position.load() * sourceRate;
    constexpr Snapshot secondControls{1.10, 1.5f, true};
    check(source.stage(*clipIdentity, restageCursor, false, secondControls),
          "new playback-rate/pitch snapshot restages outside callback");
    for (int block = 0; block < 30; ++block)
        engine.process(outputs.data(), 4, frames);
    check(source.lastEngineRenderAccepted(), "restaged rate returns to key-lock source");
    check(source.lastRenderPath() == Source::RenderPath::stretch,
          "restaged production hook returns to stretch path");

    engine.control(0).seek = 0.50;
    engine.process(outputs.data(), 4, frames);
    check(source.lastEngineRenderAccepted(), "seek remains inside safe selector boundary");
    check(source.lastRenderPath() == Source::RenderPath::fallback,
          "seek discontinuity rejects stale stretch history");
    check(source.lastFallbackReason() == Source::FallbackReason::cursorDiscontinuity,
          "seek exposes explicit cursor-discontinuity fallback reason");
    check(std::abs(engine.meter(0).position.load() - engine.meter(0).audiblePosition.load()) < 1.0e-6,
          "seek fallback publishes ordinary audible scheduling");

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

void runDeckOwnerHandoff() {
    using Owner = broke::EngineKeyLockDeckOwner;
    using Snapshot = Owner::ControlSnapshot;
    using Status = Owner::StageStatus;
    constexpr double sourceRate = 44100.0;
    constexpr double deviceRate = 48000.0;
    constexpr int frames = 512;

    Owner owner;
    auto clip = makeTone(sourceRate);
    const broke::Clip* clipIdentity = clip.get();
    constexpr Snapshot firstControls{1.0, -2.0f, true};

    check(owner.stage(*clipIdentity, 0.0, false, firstControls) == Status::notConfigured,
          "deck owner refuses staging before device configuration");
    check(!owner.configureDevice(0.0, frames), "deck owner rejects invalid device rate");
    check(owner.configureDevice(deviceRate, frames, 4.0),
          "deck owner accepts bounded device configuration while stopped");
    const auto firstGeneration = owner.generation();
    check(owner.stage(*clipIdentity, 0.0, false, firstControls) == Status::staged,
          "deck owner publishes first fully prepared key-lock snapshot");
    check(owner.armed() && owner.generation() == firstGeneration + 1,
          "first owner publication arms exactly one new generation");

    broke::Engine engine;
    engine.prepare(deviceRate, frames);
    check(engine.setDeckSourceRenderer(0, &owner), "deck owner installs once while audio is stopped");
    check(engine.submit(0, std::move(clip)), "owner-staged immutable clip submits without identity change");

    std::array<std::array<float, frames>, 4> audio{};
    std::array<float*, 4> outputs{};
    for (std::size_t channel = 0; channel < outputs.size(); ++channel)
        outputs[channel] = audio[channel].data();

    engine.process(outputs.data(), 4, frames); // adopt / paused
    engine.control(0).rate = 1.0f;
    engine.control(0).gain = 1.0f;
    engine.master = 1.0f;
    engine.crossfader = 0.0f;
    engine.control(0).playing = true;
    for (int block = 0; block < 40; ++block)
        engine.process(outputs.data(), 4, frames);

    check(owner.lastRenderAccepted(), "deck owner feeds qualified key-lock blocks into Engine");
    check(engine.meter(0).position.load() > engine.meter(0).audiblePosition.load(),
          "owner publication preserves algorithm-latency-compensated meter semantics");

    const double restageCursor = engine.meter(0).position.load() * sourceRate;
    constexpr Snapshot secondControls{1.0, 2.0f, true};
    check(owner.stage(*clipIdentity, restageCursor, false, secondControls) == Status::staged,
          "inactive slot accepts off-callback pitch restage while owner stays installed");
    for (int block = 0; block < 30; ++block)
        engine.process(outputs.data(), 4, frames);
    check(owner.lastRenderAccepted(), "atomically published pitch snapshot becomes active without reinstall");

    const auto armedGeneration = owner.generation();
    owner.disarm();
    check(!owner.armed() && owner.generation() == armedGeneration + 1,
          "disarm publishes immediate fail-closed fallback generation");
    engine.process(outputs.data(), 4, frames);
    check(!owner.lastRenderAccepted(), "disarmed owner makes Engine use built-in converter immediately");
    check(std::abs(engine.meter(0).position.load() - engine.meter(0).audiblePosition.load()) < 1.0e-6,
          "disarmed owner returns to ordinary production scheduling");
    check(std::any_of(audio[0].begin(), audio[0].end(), [](float value) {
        return std::isfinite(value) && std::abs(value) > 0.001f;
    }), "disarmed owner preserves audible production fallback");

    check(owner.stage(*clipIdentity, engine.meter(0).position.load() * sourceRate,
                      false, Snapshot{1.0, 0.0f, false}) == Status::disabled,
          "disabled stage request stays fail-closed without priming a source");

    auto replacement = makeTone(sourceRate);
    const broke::Clip* replacementIdentity = replacement.get();
    check(owner.stage(*replacementIdentity, 0.0, false, firstControls) == Status::staged,
          "replacement immutable clip can stage before mailbox publication");
    check(engine.submit(0, std::move(replacement)), "replacement clip publishes after owner stage");
    engine.process(outputs.data(), 4, frames); // adopt / paused
    engine.control(0).rate = 1.0f;
    engine.control(0).playing = true;
    for (int block = 0; block < 30; ++block)
        engine.process(outputs.data(), 4, frames);
    check(owner.lastRenderAccepted(), "pre-staged replacement clip activates only after exact identity adoption");

    check(engine.setDeckSourceRenderer(0, nullptr), "deck owner removes while audio is stopped");
    owner.resetWhenAudioStopped();
    check(!owner.configured() && !owner.armed(), "stopped-audio reset clears both owner slots");
}
}

int main() {
    try {
        runSourceBoundary();
        runDeckOwnerHandoff();
        std::cout << "PASS: " << checks << " checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
