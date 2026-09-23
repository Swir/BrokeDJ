// SPDX-License-Identifier: AGPL-3.0-only
#include "core/BeatGridPerformance.h"
#include "core/Engine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numbers>
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
std::unique_ptr<broke::Clip> sineClip(float frequencyHz, float amplitude = 0.15f,
                                     int frames = 4 * 48000) {
    auto c = std::make_unique<broke::Clip>();
    c->sampleRate = 48000;
    c->left.resize(static_cast<std::size_t>(frames));
    c->right.resize(static_cast<std::size_t>(frames));
    for (int i = 0; i < frames; ++i) {
        const auto phase = 2.0 * std::numbers::pi * static_cast<double>(frequencyHz)
            * static_cast<double>(i) / c->sampleRate;
        const float value = amplitude * static_cast<float>(std::sin(phase));
        c->left[static_cast<std::size_t>(i)] = value;
        c->right[static_cast<std::size_t>(i)] = value;
    }
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
float rmsOf(const std::array<float, 512>& samples) {
    double energy = 0.0;
    for (const float sample : samples)
        energy += static_cast<double>(sample) * static_cast<double>(sample);
    return static_cast<float>(std::sqrt(energy / static_cast<double>(samples.size())));
}
float measureEqBand(float frequencyHz, float low, float mid, float high) {
    Fixture f;
    check(f.engine.submit(0, sineClip(frequencyHz)), "EQ response sine accepted");
    f.render();
    auto& control = f.engine.control(0);
    control.gain = 1.0f;
    control.trimDb = 0.0f;
    control.low = low;
    control.mid = mid;
    control.high = high;
    control.playing = true;
    f.engine.crossfader = 0.0f;
    f.engine.master = 1.0f;
    f.render(32);
    return rmsOf(f.audio[0]);
}
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
        Fixture f; f.load(0, 0.25f, 4 * 48000);
        broke::BeatAnalysisResult analysis;
        analysis.valid = true;
        analysis.bpm = 120.0;
        analysis.beatZeroSeconds = 0.0;
        analysis.confidence = 1.0;
        analysis.segments.push_back({0.0, 120.0});
        const broke::BeatGrid grid(analysis);
        check(grid.valid(), "beat-loop fixture grid valid");
        const auto plan = broke::planBeatLoop(grid, 0.75, 4.0);
        check(plan.valid, "four-beat loop plan valid");
        check(std::abs(plan.startSeconds - 0.5) < 1.0e-9, "loop plan snaps to previous beat");
        check(std::abs(plan.endSeconds - 2.5) < 1.0e-9, "loop plan resolves beat-space endpoint");
        check(!f.engine.setLoopRegionSeconds(4, plan.startSeconds, plan.endSeconds), "invalid loop deck rejected");
        check(!f.engine.setLoopRegionSeconds(0, plan.endSeconds, plan.startSeconds), "reversed loop region rejected");
        check(!f.engine.setLoopRegionSeconds(0, std::numeric_limits<double>::quiet_NaN(), plan.endSeconds), "non-finite loop region rejected");
        f.engine.control(0).reverse = true;
        check(!f.engine.setLoopRegionSeconds(0, plan.startSeconds, plan.endSeconds), "beat loop rejects reverse mode");
        f.engine.control(0).reverse = false;
        f.engine.control(0).slip = true;
        check(!f.engine.setLoopRegionSeconds(0, plan.startSeconds, plan.endSeconds), "beat loop rejects slip mode");
        f.engine.control(0).slip = false;
        check(f.engine.setLoopRegionSeconds(0, plan.startSeconds, plan.endSeconds), "reviewed beat-loop region armed");
        check(f.engine.loopRegionEnabled(0), "loop-region state published");
        f.engine.control(0).seek = plan.startSeconds / 4.0;
        f.engine.control(0).loop = true;
        f.engine.control(0).reverse = true;
        f.engine.control(0).slip = true;
        f.engine.control(0).playing = true;
        f.render();
        check(!f.engine.control(0).reverse.load() && !f.engine.control(0).slip.load(),
              "active beat loop clears incompatible reverse and slip fail-closed");
        f.render(239);
        const double wrappedPosition = f.engine.meter(0).position.load();
        check(f.engine.control(0).playing.load(), "beat loop keeps transport playing");
        check(wrappedPosition >= plan.startSeconds && wrappedPosition < plan.endSeconds,
              "beat loop wraps inside reviewed region");
        check(std::all_of(f.audio[0].begin(), f.audio[0].end(), [](float sample) {
            return std::isfinite(sample);
        }), "beat-loop render remains finite");
        f.engine.clearLoopRegion(0);
        check(!f.engine.loopRegionEnabled(0), "beat-loop region disarmed atomically");
        f.engine.control(0).loop = false;
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
        Fixture f; f.load(0, 0.25f, 8 * 48000);
        f.engine.crossfader = 0.0f;
        f.engine.master = 1.0f;
        f.engine.control(0).gain = 1.0f;
        f.engine.control(0).headphone = true;
        f.render(32);
        const double beforeSwitch = f.engine.meter(0).position.load();
        check(beforeSwitch > 0.25, "device-switch fixture advances before reprepare");

        f.engine.prepare(44100);
        check(f.engine.control(0).playing.load(), "device reprepare preserves play intent");
        f.render();
        const double afterSwitch = f.engine.meter(0).position.load();
        check(afterSwitch > beforeSwitch, "device reprepare preserves and advances transport position");
        check(afterSwitch - beforeSwitch < 0.03, "device reprepare does not rewind or jump transport");
        check(std::all_of(f.audio[0].begin(), f.audio[0].end(), [](float sample) {
            return std::isfinite(sample);
        }), "device reprepare master output remains finite");
        check(std::all_of(f.audio[2].begin(), f.audio[2].end(), [](float sample) {
            return std::isfinite(sample);
        }), "device reprepare cue output remains finite");
    }
    {
        Fixture f; f.load(0, 0.25f, 6 * 48000);
        f.engine.control(0).playing = false;
        f.engine.control(0).seek = 0.50;
        f.render();
        f.engine.control(0).slip = true;
        f.engine.control(0).reverse = true;
        f.engine.control(0).playing = true;
        f.render(8);
        const double hiddenBeforeSwitch = f.engine.meter(0).position.load();
        const double audibleBeforeSwitch = f.engine.meter(0).audiblePosition.load();
        check(hiddenBeforeSwitch - audibleBeforeSwitch > 0.10,
              "device-switch slip fixture starts with split cursors");

        f.engine.prepare(96000);
        f.render();
        const double hiddenAfterSwitch = f.engine.meter(0).position.load();
        const double audibleAfterSwitch = f.engine.meter(0).audiblePosition.load();
        check(hiddenAfterSwitch > hiddenBeforeSwitch,
              "device reprepare preserves forward hidden slip transport");
        check(audibleAfterSwitch < audibleBeforeSwitch,
              "device reprepare preserves reverse audible slip cursor");
        check(hiddenAfterSwitch - audibleAfterSwitch > 0.10,
              "device reprepare preserves split slip transport ownership");
    }
    {
        Fixture f; f.load(0, 0.25f, 6 * 48000);
        f.engine.control(0).playing = false;
        f.engine.control(0).seek = 0.60;
        f.render();
        const double anchor = f.engine.meter(0).position.load();
        f.engine.control(0).reverse = true;
        f.engine.control(0).playing = true;
        f.render(8);
        const double transport = f.engine.meter(0).position.load();
        const double audible = f.engine.meter(0).audiblePosition.load();
        check(transport < anchor - 0.05, "reverse transport moves backward");
        check(std::abs(transport - audible) < 0.0001, "ordinary reverse keeps transport and audible cursor together");
        check(std::all_of(f.audio[0].begin(), f.audio[0].end(), [](float sample) { return std::isfinite(sample); }),
              "reverse output remains finite");
    }
    {
        Fixture f; f.load(0, 0.25f, 6 * 48000);
        f.engine.control(0).playing = false;
        f.engine.control(0).seek = 0.50;
        f.render();
        const double anchor = f.engine.meter(0).position.load();
        f.engine.control(0).slip = true;
        f.engine.control(0).reverse = true;
        f.engine.control(0).playing = true;
        f.render(8);
        const double hiddenTransport = f.engine.meter(0).position.load();
        const double audibleReverse = f.engine.meter(0).audiblePosition.load();
        check(hiddenTransport > anchor + 0.05, "slip reverse preserves a forward hidden transport");
        check(audibleReverse < anchor - 0.05, "slip reverse audible cursor moves backward");
        check(hiddenTransport - audibleReverse > 0.10, "slip exposes independent transport and audible cursors");
        f.engine.control(0).reverse = false;
        f.render();
        check(std::abs(f.engine.meter(0).position.load() - f.engine.meter(0).audiblePosition.load()) < 0.0001,
              "releasing reverse in slip rejoins uninterrupted transport");
        check(f.engine.control(0).playing.load(), "slip rejoin keeps playback running");
    }
    {
        Fixture f; f.load(0, 0.25f, 48000);
        f.engine.control(0).playing = false;
        f.engine.control(0).seek = 0.0;
        f.render();
        f.engine.control(0).reverse = true;
        f.engine.control(0).playing = true;
        f.render();
        check(!f.engine.control(0).playing.load(), "reverse at track start stops without whole-track loop");
        check(f.engine.meter(0).position.load() == 0.0, "reverse start stop remains bounded at zero");
    }
    {
        Fixture f; f.load(0, 0.25f, 48000);
        f.engine.control(0).playing = false;
        f.engine.control(0).seek = 0.0;
        f.render();
        f.engine.control(0).loop = true;
        f.engine.control(0).reverse = true;
        f.engine.control(0).playing = true;
        f.render(2);
        check(f.engine.control(0).playing.load(), "whole-track loop supports reverse wrap");
        check(f.engine.meter(0).position.load() > 0.9, "reverse whole-track loop wraps to track end");
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
        const float lowAt80 = measureEqBand(80.0f, 1.0f, 0.0f, 0.0f);
        const float midAt80 = measureEqBand(80.0f, 0.0f, 1.0f, 0.0f);
        const float highAt80 = measureEqBand(80.0f, 0.0f, 0.0f, 1.0f);
        const float lowAt1k = measureEqBand(1000.0f, 1.0f, 0.0f, 0.0f);
        const float midAt1k = measureEqBand(1000.0f, 0.0f, 1.0f, 0.0f);
        const float highAt1k = measureEqBand(1000.0f, 0.0f, 0.0f, 1.0f);
        const float lowAt8k = measureEqBand(8000.0f, 1.0f, 0.0f, 0.0f);
        const float midAt8k = measureEqBand(8000.0f, 0.0f, 1.0f, 0.0f);
        const float highAt8k = measureEqBand(8000.0f, 0.0f, 0.0f, 1.0f);
        const float unityAt1k = measureEqBand(1000.0f, 1.0f, 1.0f, 1.0f);
        const float killAt1k = measureEqBand(1000.0f, 0.0f, 0.0f, 0.0f);

        check(lowAt80 > midAt80 * 2.0f && lowAt80 > highAt80 * 8.0f,
              "low EQ band is selective at 80 Hz");
        check(midAt1k > lowAt1k * 3.0f && midAt1k > highAt1k * 2.0f,
              "mid EQ band is selective at 1 kHz");
        check(highAt8k > lowAt8k * 8.0f && highAt8k > midAt8k * 2.5f,
              "high EQ band is selective at 8 kHz");
        check(unityAt1k > 0.09f && unityAt1k < 0.12f,
              "unity EQ reconstructs nominal 1 kHz level");
        check(killAt1k < unityAt1k * 0.001f,
              "full EQ kill suppresses 1 kHz by at least 60 dB in fixture");

        Fixture f;
        check(f.engine.submit(0, sineClip(1000.0f)), "invalid EQ fixture sine accepted");
        f.render();
        auto& control = f.engine.control(0);
        control.gain = 1.0f;
        control.low = std::numeric_limits<float>::quiet_NaN();
        control.mid = std::numeric_limits<float>::infinity();
        control.high = -std::numeric_limits<float>::infinity();
        control.playing = true;
        f.engine.crossfader = 0.0f;
        f.engine.master = 1.0f;
        f.render(16);
        check(std::all_of(f.audio[0].begin(), f.audio[0].end(), [](float sample) {
            return std::isfinite(sample);
        }), "invalid EQ controls fail to finite fallback");
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
        check(!f.engine.control(0).reverse.load() && !f.engine.control(0).slip.load(),
              "replacement clears transient reverse/slip transport modes");
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
