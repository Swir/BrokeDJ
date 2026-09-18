// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <string>

namespace {
int checks = 0;
void check(bool condition, const char* name) {
    ++checks;
    if (!condition) throw std::runtime_error(name);
}
std::unique_ptr<broke::Clip> clip(float value = 0.25f, int frames = 48000) {
    auto c = std::make_unique<broke::Clip>();
    c->sampleRate = 48000;
    c->left.assign(static_cast<std::size_t>(frames), value);
    c->right = c->left;
    return c;
}
struct Fixture {
    broke::Engine engine;
    std::array<std::array<float, 512>, 4> audio{};
    std::array<float*, 4> ptrs{};
    Fixture() {
        engine.prepare(48000);
        for (std::size_t i = 0; i < ptrs.size(); ++i) ptrs[i] = audio[i].data();
    }
    void render(int count = 1, int channels = 4) {
        for (int i = 0; i < count; ++i) engine.process(ptrs.data(), channels, 512);
    }
    void load(std::size_t deck, float value = 0.25f, int frames = 48000) {
        check(engine.submit(deck, clip(value, frames)), "clip accepted");
        render();
        engine.control(deck).playing = true;
    }
};
void run() {
    {
        Fixture f;
        f.render();
        check(f.audio[0][100] == 0.0f, "empty engine is silent");
        check(!f.engine.submit(4, clip()), "invalid deck rejected");
        check(!f.engine.submit(0, nullptr), "null clip rejected");
        auto bad = clip(); bad->right.clear();
        check(!f.engine.submit(0, std::move(bad)), "unequal stereo lengths rejected");
        bool threw = false;
        try { f.engine.prepare(0); } catch (const std::invalid_argument&) { threw = true; }
        check(threw, "invalid device rate rejected");
    }
    {
        Fixture f; f.load(0);
        f.render(10);
        check(f.audio[0][400] > 0.01f, "deck produces sound");
        check(f.audio[0] == f.audio[1], "stereo channels preserved");
        check(f.engine.meter(0).position.load() > 0.09, "playhead advances");
        f.engine.crossfader = 1.0f; f.render(10);
        check(std::abs(f.audio[0][400]) < 0.0001f, "crossfader mutes A/C at right edge");
        f.engine.control(0).headphone = true; f.render();
        check(f.audio[2][400] > 0.01f, "cue bypasses crossfader");
        check(std::abs(f.audio[0][400]) < 0.0001f, "cue does not leak into main");
        f.render(1, 2);
        check(std::abs(f.audio[0][400]) < 0.0001f, "stereo-only device stays cue-isolated");
    }
    {
        Fixture f; f.load(1, 0.2f, 128); f.render();
        check(!f.engine.control(1).playing.load(), "EOF stops transport");
        f.engine.control(1).seek = 0; f.engine.control(1).loop = true;
        f.engine.control(1).playing = true; f.render(3);
        check(f.engine.control(1).playing.load(), "whole-track loop continues");
        check(f.audio[0][400] > 0.0f, "loop produces output");
        f.engine.control(1).seek = 99.0; f.render();
        check(std::isfinite(f.audio[0][400]), "out-of-range seek is bounded");
    }
    {
        Fixture f; f.load(0);
        const double normalBlock = 512.0 / 48000.0;
        const double fastBlock = 768.0 / 48000.0;
        f.engine.control(0).rate = 1.5f;
        const double before = f.engine.meter(0).position.load();
        f.render();
        const double firstDelta = f.engine.meter(0).position.load() - before;
        check(firstDelta > normalBlock && firstDelta < fastBlock, "rate change slews instead of jumping");
        f.render(12);
        const double settledBefore = f.engine.meter(0).position.load();
        f.render();
        const double settledDelta = f.engine.meter(0).position.load() - settledBefore;
        check(std::abs(settledDelta - fastBlock) < 0.0002, "smoothed rate converges to target");
        f.engine.control(0).playing = false;
        f.engine.control(0).seek = 0.5; f.render();
        check(std::abs(f.engine.meter(0).position.load() - 0.5) < 0.0001f, "normalized seek works while paused");
    }
    {
        Fixture f;
        for (std::size_t d = 0; d < broke::deckCount; ++d) f.load(d, 5.0f);
        f.engine.master = 1.0f; f.render(20);
        check(f.engine.clipped.load(), "overload is reported");
        check(std::all_of(f.audio[0].begin(), f.audio[0].end(), [](float v) { return std::isfinite(v) && std::abs(v) <= 0.98f; }), "output ceiling enforced");
        f.engine.control(0).gain = std::numeric_limits<float>::quiet_NaN();
        f.engine.control(1).echo = std::numeric_limits<float>::infinity();
        f.engine.master = std::numeric_limits<float>::quiet_NaN(); f.render(10);
        check(std::isfinite(f.audio[0][400]), "invalid controls cannot poison output");
    }
    {
        Fixture f; f.load(0); f.engine.crossfader = 0.0f;
        f.engine.control(0).low = 0; f.engine.control(0).mid = 0; f.engine.control(0).high = 0;
        f.render(12);
        check(std::abs(f.audio[0][400]) < 0.0001f, "three-band EQ kill is silent");
    }
    {
        Fixture f;
        auto c = clip(0.0f, 48000); c->left[0] = c->right[0] = 0.5f;
        check(f.engine.submit(0, std::move(c)), "impulse accepted"); f.render();
        f.engine.control(0).playing = true; f.engine.control(0).echo = 0.7f;
        f.engine.control(0).headphone = true;
        bool heardEcho = false;
        for (int b = 0; b < 25; ++b) {
            f.render();
            if (b >= 23) for (float x : f.audio[2]) heardEcho = heardEcho || x > 0.1f;
        }
        check(heardEcho, "250ms echo returns an impulse");
    }
    {
        Fixture f; f.load(0);
        check(f.engine.submit(0, clip(0.1f)), "replacement submitted"); f.render();
        check(!f.engine.control(0).playing.load(), "replacement never auto-plays");
        check(f.engine.submit(0, clip(0.2f)), "second replacement queued");
        f.render(); f.engine.collectRetired(); f.render(); f.engine.collectRetired();
        check(f.engine.meter(0).duration.load() == 1.0, "retirement backpressure recovers");
    }
    {
        Fixture f;
        std::atomic<bool> running{true};
        std::thread audio([&] { while (running.load()) f.render(); });
        for (int i = 0; i < 400; ++i) {
            check(f.engine.submit(0, clip(0.2f, 4096)), "concurrent handoff accepted");
            f.engine.control(0).playing = (i % 2 == 0);
            f.engine.control(0).seek = 0.25;
            f.engine.collectRetired();
        }
        running = false; audio.join(); f.engine.collectRetired();
        check(true, "concurrent handoffs and shutdown completed");
    }
}
}
int main() {
    try { run(); std::cout << "PASS: " << checks << " checks\n"; return EXIT_SUCCESS; }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return EXIT_FAILURE; }
}
