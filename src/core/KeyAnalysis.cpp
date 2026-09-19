// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#include "KeyAnalysis.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace broke {
namespace {

constexpr double pi = 3.141592653589793238462643383279502884;
constexpr double minimumScore = 0.35;
constexpr double minimumMargin = 0.025;
constexpr double minimumChromaVariance = 2.5e-4;
constexpr double activePitchFloor = 0.035;
constexpr int firstMidiNote = 36;
constexpr int lastMidiNote = 83;

constexpr std::array<double, 12> majorProfile{
    6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88};
constexpr std::array<double, 12> minorProfile{
    6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17};

[[nodiscard]] bool finitePositive(double value) noexcept {
    return std::isfinite(value) && value > 0.0;
}

[[nodiscard]] double safeMono(float left, float right) noexcept {
    const double l = std::isfinite(left) ? static_cast<double>(left) : 0.0;
    const double r = std::isfinite(right) ? static_cast<double>(right) : l;
    return 0.5 * (l + r);
}

[[nodiscard]] double midiFrequency(int midi) noexcept {
    return 440.0 * std::pow(2.0, (static_cast<double>(midi) - 69.0) / 12.0);
}

[[nodiscard]] double centeredCorrelation(const std::array<double, 12>& observed,
                                         const std::array<double, 12>& profile,
                                         int tonic) noexcept {
    double observedMean = 0.0;
    double profileMean = 0.0;
    for (int pitch = 0; pitch < 12; ++pitch) {
        observedMean += observed[static_cast<std::size_t>(pitch)];
        profileMean += profile[static_cast<std::size_t>((pitch - tonic + 12) % 12)];
    }
    observedMean /= 12.0;
    profileMean /= 12.0;

    double dot = 0.0;
    double observedEnergy = 0.0;
    double profileEnergy = 0.0;
    for (int pitch = 0; pitch < 12; ++pitch) {
        const double a = observed[static_cast<std::size_t>(pitch)] - observedMean;
        const double b = profile[static_cast<std::size_t>((pitch - tonic + 12) % 12)] - profileMean;
        dot += a * b;
        observedEnergy += a * a;
        profileEnergy += b * b;
    }
    if (observedEnergy <= 1.0e-18 || profileEnergy <= 1.0e-18) return 0.0;
    return dot / std::sqrt(observedEnergy * profileEnergy);
}

} // namespace

KeyAnalysisAccumulator::KeyAnalysisAccumulator(double sampleRate, KeyAnalysisOptions input)
    : options(input), sourceRate(sampleRate) {
    if (!finitePositive(sampleRate) || sampleRate < 8000.0 || sampleRate > 384000.0
        || !finitePositive(options.targetRate) || options.targetRate < 4000.0
        || options.targetRate > 12000.0
        || !finitePositive(options.maxAnalysisSeconds) || options.maxAnalysisSeconds < 2.0
        || options.maxAnalysisSeconds > 3600.0
        || options.windowSize < 1024 || options.windowSize > 8192) {
        return;
    }

    decimationStride = static_cast<std::size_t>(std::max<long long>(
        1, std::llround(sourceRate / options.targetRate)));
    analysisRate = sourceRate / static_cast<double>(decimationStride);
    if (analysisRate < 4000.0) return;

    maxDecimatedSamples = static_cast<std::size_t>(
        std::ceil(options.maxAnalysisSeconds * analysisRate));
    maxDecimatedSamples = std::max(maxDecimatedSamples, options.windowSize);
    window.reserve(options.windowSize);
    configuredFlag = true;
}

void KeyAnalysisAccumulator::processWindow() {
    if (window.size() != options.windowSize) return;

    double mean = 0.0;
    for (float sample : window) mean += static_cast<double>(sample);
    mean /= static_cast<double>(window.size());

    double energy = 0.0;
    for (float sample : window) {
        const double centered = static_cast<double>(sample) - mean;
        energy += centered * centered;
    }
    const double rms = std::sqrt(energy / static_cast<double>(window.size()));
    if (rms < 1.0e-5) {
        window.clear();
        return;
    }

    for (int midi = firstMidiNote; midi <= lastMidiNote; ++midi) {
        const double frequency = midiFrequency(midi);
        if (frequency >= analysisRate * 0.45) continue;

        const double omega = 2.0 * pi * frequency / analysisRate;
        const double coefficient = 2.0 * std::cos(omega);
        double s1 = 0.0;
        double s2 = 0.0;
        for (std::size_t i = 0; i < window.size(); ++i) {
            const double hann = 0.5 - 0.5 * std::cos(
                2.0 * pi * static_cast<double>(i)
                / static_cast<double>(window.size() - 1));
            const double input = (static_cast<double>(window[i]) - mean) * hann;
            const double s0 = input + coefficient * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        const double power = std::max(
            0.0, s1 * s1 + s2 * s2 - coefficient * s1 * s2);
        chroma[static_cast<std::size_t>(midi % 12)] += std::log1p(power);
    }

    ++windowsProcessed;
    window.clear();
}

void KeyAnalysisAccumulator::emitDecimatedSample() {
    if (decimationCount == 0 || emittedSamples >= maxDecimatedSamples) return;
    const double sample = decimationSum / static_cast<double>(decimationCount);
    window.push_back(static_cast<float>(std::clamp(sample, -1.0e6, 1.0e6)));
    ++emittedSamples;
    decimationCount = 0;
    decimationSum = 0.0;
    if (window.size() >= options.windowSize) processWindow();
}

void KeyAnalysisAccumulator::push(const float* left, const float* right, std::size_t frames) {
    if (!configuredFlag || finishedFlag || truncatedFlag || left == nullptr || frames == 0)
        return;

    for (std::size_t i = 0; i < frames; ++i) {
        if (emittedSamples >= maxDecimatedSamples) {
            truncatedFlag = true;
            break;
        }
        const float r = right != nullptr ? right[i] : left[i];
        decimationSum += safeMono(left[i], r);
        ++decimationCount;
        ++acceptedInputSamples;
        if (decimationCount >= decimationStride) emitDecimatedSample();
    }
}

MusicalKeyResult KeyAnalysisAccumulator::finish() {
    MusicalKeyResult result;
    if (!configuredFlag || finishedFlag) return result;
    finishedFlag = true;

    if (!truncatedFlag && decimationCount > 0 && emittedSamples < maxDecimatedSamples)
        emitDecimatedSample();
    if (!window.empty()) {
        if (window.size() >= options.windowSize / 2) {
            window.resize(options.windowSize, 0.0f);
            processWindow();
        } else {
            window.clear();
        }
    }

    result.analyzedSeconds = static_cast<double>(acceptedInputSamples) / sourceRate;
    result.truncated = truncatedFlag;
    if (windowsProcessed == 0) return result;

    double total = 0.0;
    for (double value : chroma) total += value;
    if (!std::isfinite(total) || total <= 1.0e-12) return result;

    std::array<double, 12> normalized{};
    int activePitchClasses = 0;
    double variance = 0.0;
    for (std::size_t i = 0; i < normalized.size(); ++i) {
        normalized[i] = chroma[i] / total;
        if (normalized[i] >= activePitchFloor) ++activePitchClasses;
        const double delta = normalized[i] - (1.0 / 12.0);
        variance += delta * delta;
    }
    variance /= 12.0;
    if (activePitchClasses < 3 || variance < minimumChromaVariance) return result;

    double bestScore = -2.0;
    double secondScore = -2.0;
    int bestTonic = -1;
    KeyMode bestMode = KeyMode::unknown;
    for (int tonic = 0; tonic < 12; ++tonic) {
        for (const auto mode : {KeyMode::major, KeyMode::minor}) {
            const auto& profile = mode == KeyMode::major ? majorProfile : minorProfile;
            const double score = centeredCorrelation(normalized, profile, tonic);
            if (score > bestScore) {
                secondScore = bestScore;
                bestScore = score;
                bestTonic = tonic;
                bestMode = mode;
            } else if (score > secondScore) {
                secondScore = score;
            }
        }
    }

    const double margin = bestScore - secondScore;
    if (!std::isfinite(bestScore) || !std::isfinite(margin)
        || bestScore < minimumScore || margin < minimumMargin) {
        return result;
    }

    result.valid = true;
    result.tonic = bestTonic;
    result.mode = bestMode;
    result.confidence = std::clamp(0.65 * bestScore + 2.0 * margin - 0.10, 0.0, 1.0);
    return result;
}

MusicalKeyResult analyzeMusicalKey(const float* left, const float* right,
                                   std::size_t frames, double sampleRate,
                                   KeyAnalysisOptions options) {
    KeyAnalysisAccumulator accumulator(sampleRate, options);
    if (!accumulator.configured()) return {};
    accumulator.push(left, right, frames);
    return accumulator.finish();
}

const char* keyName(int tonic) noexcept {
    static constexpr std::array<const char*, 12> names{
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    if (tonic < 0 || tonic >= 12) return "?";
    return names[static_cast<std::size_t>(tonic)];
}

} // namespace broke
