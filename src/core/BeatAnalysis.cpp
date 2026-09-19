// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#include "BeatAnalysis.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace broke {
namespace {

constexpr double minimumConfidence = 0.18;
constexpr std::size_t minimumEnvelopePoints = 32;
constexpr std::size_t minimumBeatPairs = 3;

[[nodiscard]] bool finitePositive(double value) noexcept {
    return std::isfinite(value) && value > 0.0;
}

[[nodiscard]] double safeSample(float value) noexcept {
    return std::isfinite(value) ? std::abs(static_cast<double>(value)) : 0.0;
}

[[nodiscard]] double normalizedCorrelation(const std::vector<double>& onset,
                                           std::size_t lag) {
    if (lag == 0 || lag >= onset.size()) return 0.0;
    double dot = 0.0;
    double a2 = 0.0;
    double b2 = 0.0;
    std::size_t contributing = 0;
    for (std::size_t i = lag; i < onset.size(); ++i) {
        const double a = onset[i];
        const double b = onset[i - lag];
        dot += a * b;
        a2 += a * a;
        b2 += b * b;
        if (a > 0.0 || b > 0.0) ++contributing;
    }
    if (contributing < minimumBeatPairs || a2 <= 1.0e-18 || b2 <= 1.0e-18) return 0.0;
    return std::clamp(dot / std::sqrt(a2 * b2), 0.0, 1.0);
}

} // namespace

BeatAnalysisAccumulator::BeatAnalysisAccumulator(double sampleRate, BeatAnalysisOptions input)
    : options(input), sourceRate(sampleRate) {
    if (!finitePositive(sampleRate)
        || sampleRate < 8000.0 || sampleRate > 384000.0
        || !finitePositive(options.minBpm) || !finitePositive(options.maxBpm)
        || options.minBpm >= options.maxBpm
        || options.minBpm < 30.0 || options.maxBpm > 300.0
        || !finitePositive(options.envelopeRate)
        || options.envelopeRate < 50.0 || options.envelopeRate > 1000.0
        || !finitePositive(options.maxAnalysisSeconds)
        || options.maxAnalysisSeconds < 2.0 || options.maxAnalysisSeconds > 14400.0) {
        return;
    }

    samplesPerEnvelope = static_cast<std::size_t>(std::llround(sourceRate / options.envelopeRate));
    samplesPerEnvelope = std::max<std::size_t>(1, samplesPerEnvelope);
    actualEnvelopeRate = sourceRate / static_cast<double>(samplesPerEnvelope);
    maxEnvelopePoints = static_cast<std::size_t>(
        std::ceil(options.maxAnalysisSeconds * actualEnvelopeRate));
    maxEnvelopePoints = std::max<std::size_t>(minimumEnvelopePoints, maxEnvelopePoints);
    envelope.reserve(std::min<std::size_t>(maxEnvelopePoints, 262144));
    configuredFlag = true;
}

void BeatAnalysisAccumulator::emitEnvelopePoint() {
    if (samplesInCurrent == 0 || envelope.size() >= maxEnvelopePoints) return;
    const double meanAbs = currentAbsSum / static_cast<double>(samplesInCurrent);
    envelope.push_back(static_cast<float>(std::clamp(meanAbs, 0.0, 1.0e6)));
    samplesInCurrent = 0;
    currentAbsSum = 0.0;
}

void BeatAnalysisAccumulator::push(const float* left, const float* right, std::size_t frames) {
    if (!configuredFlag || finishedFlag || frames == 0 || left == nullptr || truncatedFlag) return;
    for (std::size_t i = 0; i < frames; ++i) {
        if (envelope.size() >= maxEnvelopePoints) {
            truncatedFlag = true;
            break;
        }
        const double l = safeSample(left[i]);
        const double r = right != nullptr ? safeSample(right[i]) : l;
        currentAbsSum += 0.5 * (l + r);
        ++samplesInCurrent;
        ++acceptedSamples;
        if (samplesInCurrent >= samplesPerEnvelope) emitEnvelopePoint();
    }
}

BeatAnalysisResult BeatAnalysisAccumulator::finish() {
    BeatAnalysisResult result;
    if (!configuredFlag || finishedFlag) return result;
    finishedFlag = true;
    if (!truncatedFlag) emitEnvelopePoint();
    result.analyzedSeconds = static_cast<double>(acceptedSamples) / sourceRate;
    result.truncated = truncatedFlag;
    if (envelope.size() < minimumEnvelopePoints) return result;

    std::vector<double> compressed(envelope.size(), 0.0);
    for (std::size_t i = 0; i < envelope.size(); ++i)
        compressed[i] = std::log1p(12.0 * static_cast<double>(envelope[i]));

    std::vector<double> rawDiff(envelope.size(), 0.0);
    for (std::size_t i = 1; i < compressed.size(); ++i)
        rawDiff[i] = std::max(0.0, compressed[i] - compressed[i - 1]);

    const auto history = std::max<std::size_t>(1,
        static_cast<std::size_t>(std::llround(actualEnvelopeRate)));
    std::vector<double> onset(rawDiff.size(), 0.0);
    double rolling = 0.0;
    for (std::size_t i = 0; i < rawDiff.size(); ++i) {
        if (i > 0) rolling += rawDiff[i - 1];
        if (i > history) rolling -= rawDiff[i - history - 1];
        const auto count = std::min<std::size_t>(i, history);
        const double floor = count > 0 ? rolling / static_cast<double>(count) : 0.0;
        onset[i] = std::max(0.0, rawDiff[i] - 0.35 * floor);
    }

    const double onsetEnergy = std::inner_product(onset.begin(), onset.end(), onset.begin(), 0.0);
    if (onsetEnergy <= 1.0e-12) return result;

    const auto lagMin = static_cast<std::size_t>(std::max<long long>(1,
        std::llround(actualEnvelopeRate * 60.0 / options.maxBpm)));
    const auto lagMaxCandidate = static_cast<std::size_t>(std::max<long long>(1,
        std::llround(actualEnvelopeRate * 60.0 / options.minBpm)));
    const auto lagMax = std::min<std::size_t>(lagMaxCandidate, onset.size() / minimumBeatPairs);
    if (lagMin >= lagMax) return result;

    std::vector<double> scores(lagMax + 1, 0.0);
    std::size_t bestLag = 0;
    double bestScore = 0.0;
    for (std::size_t lag = lagMin; lag <= lagMax; ++lag) {
        double score = normalizedCorrelation(onset, lag);
        if (lag * 2 <= lagMax) score += 0.12 * normalizedCorrelation(onset, lag * 2);
        scores[lag] = score;
        if (score > bestScore) {
            bestScore = score;
            bestLag = lag;
        }
    }
    if (bestLag == 0 || bestScore <= 0.0) return result;

    double refinedLag = static_cast<double>(bestLag);
    if (bestLag > lagMin && bestLag < lagMax) {
        const double ym = scores[bestLag - 1];
        const double y0 = scores[bestLag];
        const double yp = scores[bestLag + 1];
        const double denominator = ym - 2.0 * y0 + yp;
        if (std::abs(denominator) > 1.0e-12) {
            const double delta = 0.5 * (ym - yp) / denominator;
            refinedLag += std::clamp(delta, -0.5, 0.5);
        }
    }

    const double bpm = 60.0 * actualEnvelopeRate / refinedLag;
    if (!std::isfinite(bpm) || bpm < options.minBpm - 0.5 || bpm > options.maxBpm + 0.5) return result;

    constexpr std::size_t phaseTolerance = 2;
    const double maxOnset = *std::max_element(onset.begin(), onset.end());
    if (maxOnset <= 1.0e-12) return result;

    const double periodSeconds = 60.0 / bpm;
    const double candidateWindowSeconds = std::min(15.0, 8.0 * periodSeconds);
    const auto candidateLimit = std::min<std::size_t>(
        onset.size(), static_cast<std::size_t>(std::ceil(candidateWindowSeconds * actualEnvelopeRate)));
    const double candidateFloor = maxOnset * 0.18;
    std::size_t bestAnchor = 0;
    double bestAnchorScore = -1.0;
    double bestAnchorSum = 0.0;
    const double expectedBeatCount = std::max(1.0,
        static_cast<double>(onset.size()) / refinedLag);

    for (std::size_t candidate = 0; candidate < candidateLimit; ++candidate) {
        if (onset[candidate] < candidateFloor) continue;
        double sum = 0.0;
        std::size_t count = 0;
        for (double target = static_cast<double>(candidate);
             target < static_cast<double>(onset.size()); target += refinedLag) {
            const auto centre = static_cast<std::size_t>(std::llround(target));
            if (centre >= onset.size()) break;
            const auto begin = centre > phaseTolerance ? centre - phaseTolerance : 0;
            const auto localEnd = std::min(onset.size() - 1, centre + phaseTolerance);
            double local = 0.0;
            for (std::size_t i = begin; i <= localEnd; ++i) local = std::max(local, onset[i]);
            sum += local;
            ++count;
        }
        if (count == 0) continue;
        const double score = sum / expectedBeatCount;
        if (score > bestAnchorScore * 1.005
            || (bestAnchorScore >= 0.0 && score >= bestAnchorScore * 0.995
                && candidate < bestAnchor)) {
            bestAnchorScore = score;
            bestAnchorSum = sum;
            bestAnchor = candidate;
        }
    }
    if (bestAnchorScore < 0.0) return result;

    const double totalOnset = std::accumulate(onset.begin(), onset.end(), 0.0);
    const double phaseStrength = totalOnset > 1.0e-12
        ? std::clamp(bestAnchorSum / totalOnset, 0.0, 1.0) : 0.0;
    const double confidence = std::clamp(0.72 * std::min(1.0, bestScore)
                                         + 0.28 * phaseStrength, 0.0, 1.0);
    if (confidence < minimumConfidence) return result;

    result.valid = true;
    result.bpm = bpm;
    result.beatZeroSeconds = static_cast<double>(bestAnchor) / actualEnvelopeRate;
    result.confidence = confidence;
    result.segments.push_back({result.beatZeroSeconds, result.bpm});
    return result;
}

BeatAnalysisResult analyzeBeatGrid(const float* left, const float* right,
                                   std::size_t frames, double sampleRate,
                                   BeatAnalysisOptions options) {
    BeatAnalysisAccumulator accumulator(sampleRate, options);
    if (!accumulator.configured()) return {};
    accumulator.push(left, right, frames);
    return accumulator.finish();
}

} // namespace broke
