// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
int checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

struct Fixture {
    broke::Engine engine;
    std::array<std::array<float, 512>, 4> audio{};
    std::array<float*, 4> out{};
    Fixture() {
        engine.prepare(48000.0);
        for (std::size_t i = 0; i < out.size(); ++i) out[i] = audio[i].data();
        engine.crossfader = 0.0f;
        engine.master = 1.0f;
    }
    void render(int blocks = 1, int channels = 4) {
        for (int i = 0; i < blocks; ++i) engine.process(out.data(), channels, 512);
    }
};

std::unique_ptr<broke::Clip> constantClip(float value, int frames = 48000) {
    auto clip = std::make_unique<broke::Clip>();
    clip->sampleRate = 48000.0;
    clip->left.assign(static_cast<std::size_t>(frames), value);
    clip->right = clip->left;
    return clip;
}

std::unique_ptr<broke::Clip> stepClip(int frames = 48000) {
    auto clip = constantClip(1.0f, frames);
    const auto half = clip->left.size() / 2;
    std::fill(clip->left.begin() + static_cast<std::ptrdiff_t>(half), clip->left.end(), -1.0f);
    clip->right = clip->left;
    return clip;
}

float peakOf(const std::array<float, 512>& samples) {
    float peak = 0.0f;
    for (const float sample : samples) peak = std::max(peak, std::abs(sample));
    return peak;
}

void run() {
    {
        Fixture f;
        check(f.engine.submit(0, constantClip(1.0f)), "constant clip accepted");
        f.render();
        f.engine.control(0).playing = true;
        f.render(20);
        const float beforePause = f.audio[0][511];
        check(beforePause > 0.1f, "playback reaches stable level");
        f.engine.control(0).playing = false;
        f.render();
        check(f.audio[0][0] > beforePause * 0.7f, "pause transition starts near previous sample");
        check(std::abs(f.audio[0][400]) < std::abs(f.audio[0][0]) * 0.25f,
            "pause transition decays instead of hard cutting");
    }
    {
        Fixture f;
        check(f.engine.submit(0, stepClip()), "step clip accepted");
        f.render();
        f.engine.control(0).playing = true;
        f.render(20);
        const float beforeSeek = f.audio[0][511];
        check(beforeSeek > 0.1f, "pre-seek signal is positive");
        f.engine.control(0).seek = 0.75;
        f.render();
        check(f.audio[0][0] > 0.0f, "seek crossfade prevents immediate polarity discontinuity");
        check(f.audio[0][400] < -0.05f, "seek reaches destination audio after transition");
    }
    {
        Fixture f;
        check(f.engine.submit(0, stepClip(4096)), "loop continuity clip accepted");
        f.render();
        f.engine.control(0).loop = true;
        f.engine.control(0).seek = 0.99;
        f.engine.control(0).playing = true;
        f.render();
        check(f.audio[0][20] < 0.0f, "loop wrap retains previous polarity at transition start");
        check(f.audio[0][400] > 0.05f, "loop wrap reaches beginning after short crossfade");
        check(f.engine.control(0).playing.load(), "loop wrap keeps transport running");
    }
    {
        Fixture f;
        check(f.engine.submit(0, stepClip()), "slip transition clip accepted");
        f.render();
        f.engine.control(0).seek = 0.51;
        f.render();
        f.engine.control(0).slip = true;
        f.engine.control(0).reverse = true;
        f.engine.control(0).playing = true;
        f.render(4);
        const float beforeRejoin = f.audio[0][511];
        check(beforeRejoin > 0.05f, "slip reverse reaches earlier positive side of step fixture");
        check(f.engine.meter(0).position.load() > f.engine.meter(0).audiblePosition.load(),
              "slip transition fixture has independent hidden/audible cursors");
        f.engine.control(0).reverse = false;
        f.render();
        check(f.audio[0][0] > 0.0f, "slip release transition begins from previous audible side");
        check(f.audio[0][400] < -0.05f, "slip release reaches uninterrupted hidden transport after crossfade");
        check(std::all_of(f.audio[0].begin(), f.audio[0].end(), [](float x) { return std::isfinite(x); }),
              "slip/reverse transition output remains finite");
    }
    {
        Fixture f;
        check(f.engine.submit(0, constantClip(0.4f)), "cue smoothing clip accepted");
        f.render();
        f.engine.control(0).playing = true;
        f.engine.control(0).headphone = true;
        f.render(12);
        const float beforeDisable = f.audio[2][511];
        check(beforeDisable > 0.05f, "cue reaches stable output");
        f.engine.control(0).headphone = false;
        f.render();
        check(f.audio[2][0] > beforeDisable * 0.7f, "cue disable starts from previous level");
        check(std::abs(f.audio[2][400]) < std::abs(f.audio[2][0]) * 0.3f,
            "cue disable fades instead of hard cutting");
    }
    {
        Fixture f;
        check(f.engine.submit(0, constantClip(0.2f)), "automation clip accepted");
        f.render();
        f.engine.control(0).playing = true;
        f.render(10);
        f.engine.control(0).low = 0.0f;
        f.engine.control(0).mid = 0.0f;
        f.engine.control(0).high = 0.0f;
        f.engine.control(0).echo = 0.7f;
        f.engine.control(0).drive = 6.0f;
        f.render();
        check(std::all_of(f.audio[0].begin(), f.audio[0].end(), [](float x) { return std::isfinite(x); }),
            "smoothed EQ and FX automation remains finite");
    }
    {
        Fixture f;
        check(f.engine.submit(0, constantClip(0.35f)), "protection reference clip accepted");
        f.render();
        f.engine.control(0).gain = 1.0f;
        f.engine.control(0).playing = true;
        f.render(20);
        const auto outputPeak = peakOf(f.audio[0]);
        check(outputPeak < 0.90f, "reference signal stays below protection knee");
        check(std::abs(outputPeak - f.engine.masterPeak.load()) < 0.002f,
            "master protection is transparent below its knee");
        check(!f.engine.clipped.load(), "reference signal does not report pre-protection overload");
    }
    {
        Fixture f;
        check(f.engine.submit(0, constantClip(1.0f)), "overload protection clip accepted");
        f.render();
        f.engine.control(0).gain = 1.5f;
        f.engine.control(0).playing = true;
        f.render(20);
        const auto outputPeak = peakOf(f.audio[0]);
        check(f.engine.masterPeak.load() > 0.98f, "meter preserves pre-protection overload evidence");
        check(f.engine.clipped.load(), "pre-protection overload remains observable");
        check(outputPeak > 0.90f && outputPeak <= 0.980001f,
            "master safety curve bounds overloaded output without a hard clamp");
        check(std::all_of(f.audio[0].begin(), f.audio[0].end(), [](float x) { return std::isfinite(x); }),
            "master safety curve keeps overload output finite");
    }
    {
        Fixture f;
        f.engine.headphoneLevel = 1.0f;
        check(f.engine.submit(0, constantClip(0.6f)), "cue isolation clip accepted");
        f.render();
        f.engine.control(0).gain = 0.0f;
        f.engine.control(0).headphone = true;
        f.engine.control(0).playing = true;
        f.render(20);
        check(peakOf(f.audio[0]) < 0.001f && peakOf(f.audio[1]) < 0.001f,
            "private cue remains absent from the master bus when channel gain is down");
        check(peakOf(f.audio[2]) > 0.2f && peakOf(f.audio[3]) > 0.2f,
            "four-channel mode keeps independent stereo cue on outputs 3 and 4");

        f.render(2, 2);
        check(peakOf(f.audio[0]) < 0.001f && peakOf(f.audio[1]) < 0.001f,
            "two-channel mode never folds private cue into master outputs");
    }
    {
        Fixture f;
        f.engine.headphoneLevel = 1.0f;
        check(f.engine.submit(0, constantClip(1.0f)), "first cue protection clip accepted");
        check(f.engine.submit(1, constantClip(1.0f)), "second cue protection clip accepted");
        f.render();
        for (std::size_t deck = 0; deck < 2; ++deck) {
            f.engine.control(deck).gain = 0.0f;
            f.engine.control(deck).headphone = true;
            f.engine.control(deck).playing = true;
        }
        f.render(20);
        check(peakOf(f.audio[2]) <= 0.980001f && peakOf(f.audio[3]) <= 0.980001f,
            "summed cue output uses the same bounded safety curve");
    }
}
}

int main() {
    try {
        run();
        std::cout << "PASS: " << checks << " quality checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
