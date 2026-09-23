// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
int checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

std::unique_ptr<broke::Clip> makeClip(float value = 0.25f, int frames = 48000) {
    auto clip = std::make_unique<broke::Clip>();
    clip->sampleRate = 48000.0;
    clip->left.assign(static_cast<std::size_t>(frames), value);
    clip->right.assign(static_cast<std::size_t>(frames), value * 0.75f);
    return clip;
}

class FakeDeckSource final : public broke::DeckSourceRenderer {
public:
    explicit FakeDeckSource(double outputRate) : deviceRate(outputRate) {}

    bool render(const broke::Clip& clip, double cursor, bool loop,
                double playbackRate, float* left, float* right,
                int frames, double& nextTransport,
                double& nextAudible) noexcept override {
        ++calls;
        lastRate = playbackRate;
        if (!succeed || left == nullptr || right == nullptr || frames <= 0
            || !std::isfinite(cursor) || !std::isfinite(playbackRate)) {
            return false;
        }
        const double length = static_cast<double>(clip.frames());
        const double step = clip.sampleRate / deviceRate * playbackRate;
        for (int frame = 0; frame < frames; ++frame) {
            const double phase = static_cast<double>(frame) * 0.011;
            left[frame] = static_cast<float>(0.20 + 0.03 * std::sin(phase));
            right[frame] = static_cast<float>(0.12 + 0.02 * std::cos(phase));
        }
        nextTransport = cursor + step * static_cast<double>(frames);
        if (loop && length > 0.0) nextTransport = std::fmod(nextTransport, length);
        else nextTransport = std::min(nextTransport, length);
        nextAudible = std::max(0.0, nextTransport - 192.0 * clip.sampleRate / deviceRate);
        return true;
    }

    bool succeed = true;
    int calls = 0;
    double lastRate = 0.0;
private:
    double deviceRate = 48000.0;
};

void run() {
    constexpr int frames = 512;
    broke::Engine engine;
    engine.prepare(48000.0, frames);
    check(engine.preparedMaxAudioBlockFrames() == frames,
          "Engine records prepared deck-source block bound");

    FakeDeckSource source(48000.0);
    check(engine.setDeckSourceRenderer(0, &source), "deck source installs while audio is stopped");
    check(!engine.setDeckSourceRenderer(broke::deckCount, &source), "invalid deck source slot rejected");

    check(engine.submit(0, makeClip()), "clip submitted");
    std::array<std::array<float, frames>, 4> audio{};
    std::array<float*, 4> outputs{};
    for (std::size_t channel = 0; channel < outputs.size(); ++channel)
        outputs[channel] = audio[channel].data();

    // First callback adopts the clip and intentionally leaves the deck paused.
    engine.process(outputs.data(), 4, frames);
    engine.control(0).gain = 1.0f;
    engine.control(0).headphone = true;
    engine.control(0).rate = 1.25f;
    engine.master = 1.0f;
    engine.crossfader = 0.0f;
    engine.headphoneLevel = 1.0f;
    engine.control(0).playing = true;

    for (int block = 0; block < 12; ++block)
        engine.process(outputs.data(), 4, frames);

    check(source.calls == 12, "source renders once per Engine block");
    check(std::abs(source.lastRate - 1.25) < 1.0e-6, "source receives deck rate snapshot");
    check(std::any_of(audio[0].begin(), audio[0].end(), [](float value) {
        return std::isfinite(value) && std::abs(value) > 0.01f;
    }), "external source reaches master through production deck processing");
    check(std::any_of(audio[2].begin(), audio[2].end(), [](float value) {
        return std::isfinite(value) && std::abs(value) > 0.01f;
    }), "external source preserves four-output cue routing");
    check(engine.meter(0).peak.load() > 0.01f, "external source feeds production peak meter");
    check(engine.meter(0).position.load() > engine.meter(0).audiblePosition.load(),
          "Engine publishes distinct transport and audible source positions");

    // Clip adoption is a source-identity discontinuity. A replacement submitted
    // while PLAY is active must fail closed to a stopped deck before any source
    // renderer sees the new clip. This prevents stale/research audio from
    // carrying across a track boundary and requires an explicit PLAY to resume.
    const int callsBeforeReplacement = source.calls;
    check(engine.submit(0, makeClip(0.10f)), "replacement clip submitted while playing");
    engine.process(outputs.data(), 4, frames);
    check(!engine.control(0).playing.load(),
          "replacement clip adoption stops deck until explicit PLAY");
    check(source.calls == callsBeforeReplacement,
          "replacement adoption does not render through stale source ownership");
    check(std::all_of(audio[0].begin(), audio[0].end(), [](float value) {
        return std::isfinite(value) && std::abs(value) < 1.0e-7f;
    }), "replacement adoption emits no stale master audio");
    check(std::all_of(audio[2].begin(), audio[2].end(), [](float value) {
        return std::isfinite(value) && std::abs(value) < 1.0e-7f;
    }), "replacement adoption emits no stale cue audio");

    engine.control(0).playing = true;
    for (int block = 0; block < 8; ++block)
        engine.process(outputs.data(), 4, frames);
    check(source.calls == callsBeforeReplacement + 8,
          "explicit PLAY resumes renderer only after replacement adoption");
    check(std::any_of(audio[0].begin(), audio[0].end(), [](float value) {
        return std::isfinite(value) && std::abs(value) > 0.01f;
    }), "replacement source becomes audible only after explicit PLAY");

    // Production EQ/FX stay after the replaceable raw source boundary.
    engine.control(0).low = 0.0f;
    engine.control(0).mid = 0.0f;
    engine.control(0).high = 0.0f;
    for (int block = 0; block < 20; ++block)
        engine.process(outputs.data(), 4, frames);
    check(std::abs(audio[0][frames - 1]) < 0.002f,
          "existing production EQ remains downstream of external source");

    // A failed provider must not silence the deck: Engine immediately returns
    // to its built-in production converter and keeps transport advancing.
    engine.control(0).low = 1.0f;
    engine.control(0).mid = 1.0f;
    engine.control(0).high = 1.0f;
    engine.control(0).seek = 0.0;
    source.succeed = false;
    for (int block = 0; block < 16; ++block)
        engine.process(outputs.data(), 4, frames);
    check(source.calls >= 28, "failed provider continues to be queried without callback mutation");
    check(engine.meter(0).position.load() > 0.05,
          "built-in converter advances from seek after provider refusal");
    check(std::any_of(audio[0].begin(), audio[0].end(), [](float value) {
        return std::isfinite(value) && std::abs(value) > 0.01f;
    }), "provider refusal falls back to audible production playback");
    check(std::abs(engine.meter(0).position.load() - engine.meter(0).audiblePosition.load()) < 1.0e-6,
          "production fallback publishes audible position equal to transport");

    // The optional source owns the deck while installed, so production beat
    // loops are deliberately unavailable. Removing it must restore the built-in
    // loop owner rather than leaving a null slot that also disables Reverse.
    check(!engine.setLoopRegionSeconds(0, 0.10, 0.20),
          "external source keeps built-in beat-loop owner fail-closed");
    check(engine.setDeckSourceRenderer(0, nullptr),
          "deck source can be removed while audio is stopped");
    check(engine.setLoopRegionSeconds(0, 0.10, 0.20),
          "removing external source restores built-in beat-loop owner");
    engine.clearLoopRegion(0);

    engine.control(0).reverse = true;
    engine.process(outputs.data(), 4, frames);
    check(engine.control(0).reverse.load(),
          "removing external source restores production Reverse ownership");
    engine.control(0).reverse = false;
    engine.control(0).playing = false;

    bool threw = false;
    try { engine.prepare(48000.0, broke::defaultMaxAudioBlockFrames + 1); }
    catch (const std::invalid_argument&) { threw = true; }
    check(threw, "unbounded source scratch request rejected");
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
