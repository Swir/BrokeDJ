// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#include "BeatAnalysis.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <utility>

namespace broke {
namespace {

constexpr double minimumConfidence = 0.18;
constexpr double minimumPhaseStrength = 0.12;
constexpr std::size_t minimumEnvelopePoints = 32;
constexpr std::size_t minimumBeatPairs = 3;
constexpr double gridTimeEpsilon = 1.0e-8;

[[nodiscard]] bool finitePositive(double value) noexcept {
    return std::isfinite(value) && value > 0.0;
}

[[nodiscard]] double safeSample(float value) noexcept {
    return std::isfinite(value) ? std::abs(static_cast<double>(value)) : 0.0;
}

[[nodiscard]] double normalizedCorrelation(const std::vector<double>& signal,
                                           std::size_t lag) {
    if (lag == 0 || lag >= signal.size() || signal.size() - lag < minimumBeatPairs)
        return 0.0;
    double dot = 0.0;
    double a2 = 0.0;
    double b2 = 0.0;
    for (std::size_t i = lag; i < signal.size(); ++i) {
        const double a = signal[i];
        const double b = signal[i - lag];
        dot += a * b;
        a2 += a * a;
        b2 += b * b;
    }
    if (a2 <= 1.0e-18 || b2 <= 1.0e-18) return 0.0;
    return std::clamp(dot / std::sqrt(a2 * b2), 0.0, 1.0);
}

[[nodiscard]] double invalidGridValue() noexcept {
    return std::numeric_limits<double>::quiet_NaN();
}

} // namespace

BeatGrid::BeatGrid(const BeatAnalysisResult& analysis) {
    static_cast<void>(reset(analysis));
}

bool BeatGrid::validBpm(double bpm) noexcept {
    return std::isfinite(bpm) && bpm >= 30.0 && bpm <= 300.0;
}

bool BeatGrid::validate() const noexcept {
    if (!std::isfinite(beatZero) || beatZero < 0.0
        || tempoMap.empty() || tempoMap.size() > maxSegments
        || !std::isfinite(tempoMap.front().startSeconds)
        || std::abs(tempoMap.front().startSeconds - beatZero) > gridTimeEpsilon)
        return false;

    double previousStart = beatZero - 1.0;
    for (std::size_t i = 0; i < tempoMap.size(); ++i) {
        const auto& segment = tempoMap[i];
        if (!std::isfinite(segment.startSeconds) || segment.startSeconds < beatZero
            || !validBpm(segment.bpm))
            return false;
        if (i > 0 && segment.startSeconds <= previousStart + gridTimeEpsilon)
            return false;
        previousStart = segment.startSeconds;
    }
    return true;
}

bool BeatGrid::valid() const noexcept {
    return validate();
}

bool BeatGrid::reset(double beatZeroSecondsIn, double bpm) {
    BeatGrid candidate;
    candidate.beatZero = beatZeroSecondsIn;
    candidate.tempoMap.push_back({beatZeroSecondsIn, bpm});
    if (!candidate.validate()) return false;
    *this = std::move(candidate);
    return true;
}

bool BeatGrid::reset(const BeatAnalysisResult& analysis) {
    if (!analysis.valid || !std::isfinite(analysis.beatZeroSeconds)
        || analysis.beatZeroSeconds < 0.0 || !validBpm(analysis.bpm))
        return false;

    BeatGrid candidate;
    candidate.beatZero = analysis.beatZeroSeconds;
    if (analysis.segments.empty()) {
        candidate.tempoMap.push_back({analysis.beatZeroSeconds, analysis.bpm});
    } else {
        if (analysis.segments.size() > maxSegments) return false;
        candidate.tempoMap = analysis.segments;
    }
    if (!candidate.validate()) return false;
    *this = std::move(candidate);
    return true;
}

bool BeatGrid::setBeatZero(double seconds) {
    if (!validate() || !std::isfinite(seconds) || seconds < 0.0) return false;
    BeatGrid candidate = *this;
    const double delta = seconds - candidate.beatZero;
    candidate.beatZero = seconds;
    for (auto& segment : candidate.tempoMap)
        segment.startSeconds += delta;
    if (!candidate.validate()) return false;
    *this = std::move(candidate);
    return true;
}

bool BeatGrid::setSegmentBpm(std::size_t index, double bpm) {
    if (!validate() || index >= tempoMap.size() || !validBpm(bpm)) return false;
    BeatGrid candidate = *this;
    candidate.tempoMap[index].bpm = bpm;
    if (!candidate.validate()) return false;
    *this = std::move(candidate);
    return true;
}

bool BeatGrid::insertTempoChangeAtBeat(double beat, double bpm) {
    if (!validate() || !std::isfinite(beat) || beat <= 0.0 || !validBpm(bpm)
        || tempoMap.size() >= maxSegments)
        return false;

    const double seconds = timeAtBeat(beat);
    if (!std::isfinite(seconds) || seconds <= beatZero + gridTimeEpsilon) return false;

    const auto insertion = std::lower_bound(
        tempoMap.begin(), tempoMap.end(), seconds,
        [](const BeatGridSegment& segment, double value) {
            return segment.startSeconds < value;
        });
    if (insertion != tempoMap.end()
        && std::abs(insertion->startSeconds - seconds) <= gridTimeEpsilon)
        return false;
    if (insertion != tempoMap.begin()) {
        const auto previous = std::prev(insertion);
        if (std::abs(previous->startSeconds - seconds) <= gridTimeEpsilon)
            return false;
    }

    BeatGrid candidate = *this;
    const auto offset = static_cast<std::size_t>(std::distance(tempoMap.begin(), insertion));
    candidate.tempoMap.insert(candidate.tempoMap.begin() + static_cast<std::ptrdiff_t>(offset),
                              {seconds, bpm});
    if (!candidate.validate()) return false;
    *this = std::move(candidate);
    return true;
}

bool BeatGrid::removeTempoChange(std::size_t index) {
    if (!validate() || index == 0 || index >= tempoMap.size()) return false;
    BeatGrid candidate = *this;
    candidate.tempoMap.erase(candidate.tempoMap.begin() + static_cast<std::ptrdiff_t>(index));
    if (!candidate.validate()) return false;
    *this = std::move(candidate);
    return true;
}

double BeatGrid::beatAtTime(double seconds) const noexcept {
    if (!validate() || !std::isfinite(seconds)) return invalidGridValue();
    if (seconds < beatZero)
        return (seconds - beatZero) * tempoMap.front().bpm / 60.0;

    double beats = 0.0;
    for (std::size_t i = 0; i < tempoMap.size(); ++i) {
        const auto& segment = tempoMap[i];
        if (i + 1 == tempoMap.size())
            return beats + (seconds - segment.startSeconds) * segment.bpm / 60.0;

        const double nextStart = tempoMap[i + 1].startSeconds;
        if (seconds < nextStart)
            return beats + (seconds - segment.startSeconds) * segment.bpm / 60.0;
        beats += (nextStart - segment.startSeconds) * segment.bpm / 60.0;
    }
    return invalidGridValue();
}

double BeatGrid::timeAtBeat(double beat) const noexcept {
    if (!validate() || !std::isfinite(beat)) return invalidGridValue();
    if (beat < 0.0)
        return beatZero + beat * 60.0 / tempoMap.front().bpm;

    double beats = 0.0;
    for (std::size_t i = 0; i < tempoMap.size(); ++i) {
        const auto& segment = tempoMap[i];
        if (i + 1 == tempoMap.size())
            return segment.startSeconds + (beat - beats) * 60.0 / segment.bpm;

        const double nextStart = tempoMap[i + 1].startSeconds;
        const double segmentBeats = (nextStart - segment.startSeconds) * segment.bpm / 60.0;
        if (beat <= beats + segmentBeats)
            return segment.startSeconds + (beat - beats) * 60.0 / segment.bpm;
        beats += segmentBeats;
    }
    return invalidGridValue();
}

double BeatGrid::quantizeTime(double seconds, double beatStep) const noexcept {
    if (!validate() || !std::isfinite(seconds) || !std::isfinite(beatStep)
        || beatStep <= 0.0 || beatStep > 64.0)
        return invalidGridValue();
    const double beat = beatAtTime(seconds);
    if (!std::isfinite(beat)) return invalidGridValue();
    const double quantizedBeat = std::round(beat / beatStep) * beatStep;
    return timeAtBeat(quantizedBeat);
}

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

    // Autocorrelating a non-negative onset envelope without removing its DC
    // component can make broadband/noisy material appear periodic. Keep the
    // positive onset envelope for phase anchoring, but use a centered copy for
    // tempo scoring so the confidence reflects repeated structure rather than
    // the envelope's mean level.
    const double onsetMean = std::accumulate(onset.begin(), onset.end(), 0.0)
        / static_cast<double>(onset.size());
    std::vector<double> correlationSignal(onset.size(), 0.0);
    for (std::size_t i = 0; i < onset.size(); ++i)
        correlationSignal[i] = onset[i] - onsetMean;

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
        double score = normalizedCorrelation(correlationSignal, lag);
        if (lag * 2 <= lagMax)
            score += 0.12 * normalizedCorrelation(correlationSignal, lag * 2);
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
    if (!std::isfinite(bpm) || bpm < options.minBpm - 0.5 || bpm > options.maxBpm + 0.5)
        return result;

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
            for (std::size_t i = begin; i <= localEnd; ++i)
                local = std::max(local, onset[i]);
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
    // Strong autocorrelation alone is not sufficient: stationary tones can
    // generate deterministic envelope beating from the finite analysis window.
    // Require a meaningful fraction of onset energy to align with the proposed
    // beat grid before publishing musical tempo metadata.
    if (phaseStrength < minimumPhaseStrength) return result;
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
