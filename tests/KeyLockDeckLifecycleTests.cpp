// SPDX-License-Identifier: AGPL-3.0-only
#include "app/KeyLockDeckLifecycle.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace {
#define CHECK(expr) do { if (!(expr)) { std::cerr << "CHECK failed: " #expr " at line " << __LINE__ << '\n'; std::exit(1); } } while (false)

std::unique_ptr<broke::Clip> makeClip(double sampleRate = 48000.0, int frames = 48000,
                                      double frequency = 440.0) {
    auto clip = std::make_unique<broke::Clip>();
    clip->sampleRate = sampleRate;
    clip->left.resize(static_cast<std::size_t>(frames));
    clip->right.resize(static_cast<std::size_t>(frames));
    for (int i = 0; i < frames; ++i) {
        const float sample = static_cast<float>(0.2 * std::sin(2.0 * 3.14159265358979323846
            * frequency * static_cast<double>(i) / sampleRate));
        clip->left[static_cast<std::size_t>(i)] = sample;
        clip->right[static_cast<std::size_t>(i)] = sample;
    }
    return clip;
}

void process(broke::Engine& engine, int frames = 128) {
    std::array<float, 128> left{};
    std::array<float, 128> right{};
    std::array<float*, 2> channels{left.data(), right.data()};
    CHECK(frames <= static_cast<int>(left.size()));
    engine.process(channels.data(), 2, frames);
}
}

int main() {
    broke::Engine engine;
    engine.prepare(48000.0, 256);
    broke::KeyLockDeckLifecycle lifecycle(engine);

    CHECK(lifecycle.configureAudioStopped(48000.0, 256));
    CHECK(lifecycle.configured());
    CHECK(lifecycle.setEnabled(0, true));

    auto clip = makeClip();
    auto* clipAddress = clip.get();
    CHECK(engine.submit(0, std::move(clip)));
    lifecycle.noteClipSubmitted(0, clipAddress);
    CHECK(lifecycle.dirty(0));

    // Engine adopts a newly submitted clip at the next callback boundary and,
    // by design, forces its play control back to stopped. Stage/play assertions
    // therefore start only after this ordinary production adoption block.
    process(engine);
    CHECK(!engine.control(0).playing.load());
    CHECK(lifecycle.service(0, 0.0, false, 1.0, false)
        == broke::KeyLockDeckLifecycle::ServiceStatus::staged);
    CHECK(lifecycle.armed(0));
    const auto stableGeneration = lifecycle.generation(0);
    CHECK(lifecycle.service(0, 0.0, false, 1.0, false)
        == broke::KeyLockDeckLifecycle::ServiceStatus::idle);
    CHECK(lifecycle.generation(0) == stableGeneration);

    engine.control(0).playing.store(true);
    process(engine);
    CHECK(lifecycle.lastRenderAccepted(0));

    // A live rate/loop-style discontinuity fails closed immediately and may not
    // re-prime while the deck is running. Engine must continue via production fallback.
    engine.control(0).rate.store(1.25f);
    lifecycle.noteTransportControlChanged(0);
    CHECK(!lifecycle.armed(0));
    CHECK(lifecycle.service(0, engine.meter(0).position.load(), false, 1.25, true)
        == broke::KeyLockDeckLifecycle::ServiceStatus::deferredWhilePlaying);
    process(engine);
    CHECK(!lifecycle.lastRenderAccepted(0));

    engine.control(0).playing.store(false);
    process(engine);
    CHECK(lifecycle.service(0, engine.meter(0).position.load(), false, 1.25, false)
        == broke::KeyLockDeckLifecycle::ServiceStatus::staged);
    CHECK(lifecycle.armed(0));
    engine.control(0).playing.store(true);
    process(engine);
    CHECK(lifecycle.lastRenderAccepted(0));

    // Authoritative service values also protect against a controller/device path
    // that changes Engine controls without first notifying the UI adapter. The
    // stale snapshot must disarm while live, then become stageable once paused.
    engine.control(0).rate.store(1.10f);
    CHECK(lifecycle.service(0, engine.meter(0).position.load(), false, 1.10, true)
        == broke::KeyLockDeckLifecycle::ServiceStatus::deferredWhilePlaying);
    CHECK(!lifecycle.armed(0));
    CHECK(lifecycle.dirty(0));
    process(engine);
    CHECK(!lifecycle.lastRenderAccepted(0));
    engine.control(0).playing.store(false);
    process(engine);
    CHECK(lifecycle.service(0, engine.meter(0).position.load(), false, 1.10, false)
        == broke::KeyLockDeckLifecycle::ServiceStatus::staged);
    CHECK(lifecycle.armed(0));

    // The native timer reads the float Engine control and passes it back as a
    // double. That harmless representation round-trip must not cause repeated
    // restaging or generation churn.
    const auto tolerantGeneration = lifecycle.generation(0);
    const double rateFromEngineFloat = static_cast<double>(engine.control(0).rate.load());
    CHECK(lifecycle.service(0, engine.meter(0).position.load(), false,
                            rateFromEngineFloat, false)
        == broke::KeyLockDeckLifecycle::ServiceStatus::idle);
    CHECK(lifecycle.armed(0));
    CHECK(lifecycle.generation(0) == tolerantGeneration);

    // The same self-healing boundary covers loop ownership. A loop change that
    // bypasses the explicit notification path cannot leave key lock permanently
    // stale/armed; the owner fails closed and restages only while paused.
    engine.control(0).playing.store(true);
    engine.control(0).loop.store(true);
    CHECK(lifecycle.service(0, engine.meter(0).position.load(), true, 1.10, true)
        == broke::KeyLockDeckLifecycle::ServiceStatus::deferredWhilePlaying);
    CHECK(!lifecycle.armed(0));
    engine.control(0).playing.store(false);
    process(engine);
    CHECK(lifecycle.service(0, engine.meter(0).position.load(), true, 1.10, false)
        == broke::KeyLockDeckLifecycle::ServiceStatus::staged);
    CHECK(lifecycle.armed(0));

    // Seek is staged against an explicit immutable-clip cursor while paused.
    engine.control(0).playing.store(false);
    process(engine);
    engine.control(0).seek.store(0.5);
    lifecycle.noteSeekNormalized(0, 0.5);
    CHECK(!lifecycle.armed(0));
    CHECK(lifecycle.service(0, engine.meter(0).position.load(), true, 1.10, false)
        == broke::KeyLockDeckLifecycle::ServiceStatus::staged);
    process(engine); // consumes Engine's seek mailbox while still paused
    engine.control(0).playing.store(true);
    process(engine);
    CHECK(lifecycle.lastRenderAccepted(0));
    CHECK(engine.meter(0).position.load() > 0.45);

    // Pitch intent is non-realtime and must also disarm/restage before use.
    engine.control(0).playing.store(false);
    process(engine);
    CHECK(lifecycle.setPitchSemitones(0, 3.0f));
    CHECK(!lifecycle.armed(0));
    CHECK(lifecycle.service(0, engine.meter(0).position.load(), true, 1.10, false)
        == broke::KeyLockDeckLifecycle::ServiceStatus::staged);
    engine.control(0).playing.store(true);
    process(engine);
    CHECK(lifecycle.lastRenderAccepted(0));

    // Clip replacement while live is another ownership transition: the old
    // renderer is disarmed immediately, the Engine adopts the immutable new
    // clip on its normal callback boundary, and only the paused deck may restage.
    auto replacement = makeClip(48000.0, 48000, 660.0);
    auto* replacementAddress = replacement.get();
    CHECK(engine.submit(0, std::move(replacement)));
    lifecycle.noteClipSubmitted(0, replacementAddress);
    CHECK(!lifecycle.armed(0));
    CHECK(lifecycle.dirty(0));
    CHECK(lifecycle.service(0, engine.meter(0).position.load(), true, 1.10, true)
        == broke::KeyLockDeckLifecycle::ServiceStatus::deferredWhilePlaying);
    process(engine); // adopts replacement and forces ordinary transport stopped
    CHECK(!engine.control(0).playing.load());
    CHECK(lifecycle.service(0, engine.meter(0).position.load(), true, 1.10, false)
        == broke::KeyLockDeckLifecycle::ServiceStatus::staged);
    engine.control(0).playing.store(true);
    process(engine);
    CHECK(lifecycle.lastRenderAccepted(0));

    // Invalid pitch is fail-closed and requires a later valid restage.
    CHECK(!lifecycle.setPitchSemitones(0, 30.0f));
    CHECK(!lifecycle.armed(0));
    CHECK(lifecycle.dirty(0));

    engine.control(0).playing.store(false);
    process(engine);
    CHECK(lifecycle.setPitchSemitones(0, 0.0f));
    engine.control(0).rate.store(1.0f);
    CHECK(lifecycle.service(0, engine.meter(0).position.load(), true, 1.0, false)
        == broke::KeyLockDeckLifecycle::ServiceStatus::staged);

    const auto generationBeforeRelease = lifecycle.generation(0);
    lifecycle.releaseAudioStopped();
    CHECK(!lifecycle.configured());
    CHECK(!lifecycle.armed(0));
    CHECK(lifecycle.generation(0) >= generationBeforeRelease);

    // A failed device configuration remains fail-closed, but the immutable clip
    // intent survives so the next valid stopped-audio prepare can recover.
    CHECK(!lifecycle.configureAudioStopped(1000.0, 256));
    CHECK(!lifecycle.configured());
    CHECK(!lifecycle.armed(0));
    CHECK(lifecycle.dirty(0));
    CHECK(lifecycle.service(0, engine.meter(0).position.load(), true, 1.0, false)
        == broke::KeyLockDeckLifecycle::ServiceStatus::notConfigured);

    // Device re-prepare preserves immutable clip intent but never stale DSP state.
    engine.prepare(44100.0, 256);
    CHECK(lifecycle.configureAudioStopped(44100.0, 256));
    CHECK(lifecycle.dirty(0));
    CHECK(lifecycle.service(0, engine.meter(0).position.load(), true, 1.0, false)
        == broke::KeyLockDeckLifecycle::ServiceStatus::staged);
    CHECK(lifecycle.armed(0));

    lifecycle.releaseAudioStopped();
    std::cout << "Key-lock native lifecycle tests passed\n";
    return 0;
}
