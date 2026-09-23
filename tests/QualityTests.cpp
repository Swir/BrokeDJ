// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
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

std::unique_ptr<broke::Clip> constantClip(float value, int frames = 48000,
                                          double sampleRate = 48000.0) {
    auto clip = std::make_unique<broke::Clip>();
    clip->sampleRate = sampleRate;
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
        check(std::abs(broke::decibelsToGain(0.0f) - 1.0f) < 1.0e-6f,
              "zero dB trim resolves to unity gain");
        check(std::abs(broke::decibelsToGain(6.0f) - 1.9952623f) < 1.0e-5f,
              "+6 dB trim uses the expected amplitude ratio");
        check(std::abs(broke::decibelsToGain(-6.0f) - 0.5011872f) < 1.0e-5f,
              "-6 dB trim uses the expected amplitude ratio");
        check(std::abs(broke::decibelsToGain(std::numeric_limits<float>::quiet_NaN()) - 1.0f) < 1.0e-6f,
              "non-finite trim fails safe to unity");
        check(std::abs(broke::decibelsToGain(100.0f) - broke::decibelsToGain(broke::maxTrimDb)) < 1.0e-6f
                  && std::abs(broke::decibelsToGain(-100.0f) - broke::decibelsToGain(broke::minTrimDb)) < 1.0e-6f,
              "trim conversion clamps to the documented safe range");
    }
    {
        const auto constant = broke::crossfaderGains(0.5f, broke::CrossfaderCurve::constantPower);
        const auto linear = broke::crossfaderGains(0.5f, broke::CrossfaderCurve::linear);
        const auto cutLeft = broke::crossfaderGains(0.34f, broke::CrossfaderCurve::fastCut);
        const auto cutRight = broke::crossfaderGains(0.66f, broke::CrossfaderCurve::fastCut);
        check(std::abs(constant.left - 0.70710678f) < 1.0e-5f
                  && std::abs(constant.right - 0.70710678f) < 1.0e-5f,
              "constant-power crossfader keeps equal-power centre gains");
        check(std::abs(linear.left - 0.5f) < 1.0e-6f
                  && std::abs(linear.right - 0.5f) < 1.0e-6f,
              "linear crossfader uses half-gain centre");
        check(cutLeft.left > 0.999f && cutLeft.right < 0.001f
                  && cutRight.left < 0.001f && cutRight.right > 0.999f,
              "fast-cut crossfader narrows the blend region without endpoint leakage");
        const auto sanitized = broke::crossfaderCurveFromRaw(255);
        check(sanitized == broke::CrossfaderCurve::constantPower,
              "unknown crossfader curve fails safe to constant power");
        const auto nonFinite = broke::crossfaderGains(
            std::numeric_limits<float>::quiet_NaN(), broke::CrossfaderCurve::linear);
        check(std::isfinite(nonFinite.left) && std::isfinite(nonFinite.right)
                  && std::abs(nonFinite.left - 0.5f) < 1.0e-6f,
              "non-finite crossfader position resolves to a finite centre state");
    }
    {
        Fixture f;
        check(f.engine.submit(0, constantClip(0.35f)), "crossfader curve clip accepted");
        f.render();
        f.engine.control(0).gain = 1.0f;
        f.engine.control(0).playing = true;
        f.engine.crossfader = 0.5f;
        f.engine.crossfaderCurve = static_cast<std::uint8_t>(broke::CrossfaderCurve::constantPower);
        f.render(24);
        const float constantPeak = peakOf(f.audio[0]);
        check(constantPeak > 0.20f && constantPeak < 0.30f,
              "constant-power centre reaches expected left-deck level");

        const float beforeModeChange = f.audio[0][511];
        f.engine.crossfaderCurve = static_cast<std::uint8_t>(broke::CrossfaderCurve::linear);
        f.render();
        check(std::abs(f.audio[0][0] - beforeModeChange) < 0.02f,
              "live crossfader-curve change is gain-smoothed instead of discontinuous");
        f.render(24);
        const float linearPeak = peakOf(f.audio[0]);
        check(linearPeak < constantPeak * 0.80f,
              "linear centre is measurably lower than constant-power centre");

        f.engine.crossfader = 0.40f;
        f.engine.crossfaderCurve = static_cast<std::uint8_t>(broke::CrossfaderCurve::fastCut);
        f.render(24);
        const float fastCutPeak = peakOf(f.audio[0]);
        check(fastCutPeak > linearPeak * 1.45f,
              "fast-cut curve materially favors the active side inside its narrow blend zone");
        check(std::all_of(f.audio[0].begin(), f.audio[0].end(), [](float x) { return std::isfinite(x); }),
              "crossfader curve automation remains finite");
    }
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
        f.engine.control(0).trimDb = 12.0f;
        f.render();
        check(std::all_of(f.audio[0].begin(), f.audio[0].end(), [](float x) { return std::isfinite(x); }),
            "smoothed trim, EQ and FX automation remains finite");
    }
    {
        // The largest supported device rate makes the fixed 250 ms echo ring
        // 48,000 frames per channel. Replacement must not leak the old deck's
        // echo tail and the adoption callback records diagnostic cost without
        // turning shared-runner timing into a hardware-performance threshold.
        constexpr double sampleRate = 192000.0;
        broke::Engine engine;
        engine.prepare(sampleRate);
        engine.crossfader = 0.0f;
        engine.master = 1.0f;
        std::array<std::array<float, 512>, 4> audio{};
        std::array<float*, 4> out{};
        for (std::size_t channel = 0; channel < out.size(); ++channel)
            out[channel] = audio[channel].data();

        check(engine.submit(0, constantClip(0.40f, 192000, sampleRate)),
              "max-rate echo adoption fixture accepted");
        engine.process(out.data(), 4, 512);
        auto& control = engine.control(0);
        control.gain = 1.0f;
        control.echo = 0.7f;
        control.playing = true;
        for (int block = 0; block < 110; ++block) engine.process(out.data(), 4, 512);
        check(peakOf(audio[0]) > 0.05f,
              "max-rate echo fixture reaches audible old-deck state before replacement");

        check(engine.submit(0, constantClip(0.0f, 192000, sampleRate)),
              "silent replacement accepted after populated echo history");
        const auto adoptionStarted = std::chrono::steady_clock::now();
        engine.process(out.data(), 4, 512);
        const auto adoptionFinished = std::chrono::steady_clock::now();
        engine.collectRetired();

        control.gain = 1.0f;
        control.echo = 0.7f;
        control.playing = true;
        for (int block = 0; block < 8; ++block) engine.process(out.data(), 4, 512);
        const float staleEchoPeak = peakOf(audio[0]);
        check(staleEchoPeak < 1.0e-6f,
              "clip replacement cannot leak stale echo history into the new deck epoch");
        const auto adoptionNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            adoptionFinished - adoptionStarted).count();
        std::cout << "METRIC deck_adoption_output_hz=192000 delay_frames=48000"
                  << " adoption_elapsed_ns=" << adoptionNs
                  << " stale_echo_peak=" << staleEchoPeak
                  << " timing_is_diagnostic_only=1\n";
    }
    {
        Fixture f;
        check(f.engine.submit(0, constantClip(0.25f)), "gain staging reference clip accepted");
        f.render();
        auto& control = f.engine.control(0);
        control.gain = 1.0f;
        control.trimDb = 0.0f;
        control.playing = true;
        f.render(24);
        const float unityPre = f.engine.meter(0).preFaderPeak.load();
        const float unityRms = f.engine.meter(0).rms.load();
        check(unityPre > 0.20f && unityPre < 0.30f,
              "pre-fader meter observes the stable unity-trim channel signal");
        check(unityRms > 0.20f && unityRms <= f.engine.meter(0).peak.load() + 0.001f,
              "post-fader RMS is finite and bounded by the channel peak for a constant signal");

        control.trimDb = 6.0f;
        f.render(24);
        const float boostedPre = f.engine.meter(0).preFaderPeak.load();
        check(boostedPre > unityPre * 1.85f && boostedPre < unityPre * 2.10f,
              "+6 dB input trim produces the expected near-2x pre-fader level");
        check(f.engine.masterRms.load() > unityRms * 1.80f,
              "master RMS tracks the gain-staging change below protection range");

        control.trimDb = std::numeric_limits<float>::quiet_NaN();
        f.render(24);
        const float fallbackPre = f.engine.meter(0).preFaderPeak.load();
        check(std::abs(fallbackPre - unityPre) < 0.01f,
              "non-finite runtime trim falls back to unity instead of poisoning audio");
        check(!f.engine.meter(0).overloaded.load(),
              "normal gain-staging reference does not report channel overload");
    }
    {
        Fixture f;
        check(f.engine.submit(0, constantClip(0.40f)), "channel overload fixture accepted");
        f.render();
        auto& control = f.engine.control(0);
        control.gain = 0.20f;
        control.trimDb = 12.0f;
        control.playing = true;
        f.render(24);
        check(f.engine.meter(0).preFaderPeak.load() > 1.0f,
              "input trim meter preserves pre-fader overload evidence");
        check(f.engine.meter(0).overloaded.load(),
              "channel overload flag is independent from the post-fader master level");
        check(!f.engine.clipped.load(),
              "lowering the channel fader can prevent master overload without hiding the channel trim warning");
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

        f.engine.control(0).trimDb = -12.0f;
        f.render(24);
        check(peakOf(f.audio[2]) < 0.25f,
              "pre-fader cue follows input trim while remaining independent of the channel fader");

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
