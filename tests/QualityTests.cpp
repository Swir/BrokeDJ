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
    void render(int blocks = 1) {
        for (int i = 0; i < blocks; ++i) engine.process(out.data(), 4, 512);
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
