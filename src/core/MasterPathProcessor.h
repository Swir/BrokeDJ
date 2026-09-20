// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>

namespace broke {

// Allocation-free post-mix processor for the native app's master 1/2 path.
// It intentionally stays JUCE-independent so microphone ducking, linked-stereo
// safety limiting and dedicated booth routing can be exercised deterministically
// in tests.
//
// This is not a transparent mastering limiter: it has no look-ahead or
// true-peak reconstruction. It is a bounded zero-attack sample-peak safety
// stage with a smoothed release, intended to prevent runaway digital peaks
// while preserving explicit overload/gain-reduction evidence.
class MasterPathProcessor final {
public:
    struct Snapshot final {
        bool microphoneEnabled = false;
        bool limiterEnabled = true;
        bool boothEnabled = false;
        float microphoneEnvelope = 0.0f;
        float duckGain = 1.0f;
        float limiterGain = 1.0f;
        float boothGain = 1.0f;
        float maxInputPeak = 0.0f;
        float maxOutputPeak = 0.0f;
        float maxBoothPeak = 0.0f;
        float maxGainReductionDb = 0.0f;
    };

    static_assert(std::atomic<float>::is_always_lock_free,
                  "BrokeDJ master-path controls require lock-free float atomics");
    static_assert(std::atomic<bool>::is_always_lock_free,
                  "BrokeDJ master-path controls require lock-free bool atomics");

    void prepare(double rate) noexcept {
        sampleRate = std::isfinite(rate) && rate >= 8000.0 && rate <= 384000.0
            ? rate : 48000.0;
        micAttackCoeff = static_cast<float>(coefficientForMilliseconds(5.0));
        micReleaseCoeff = static_cast<float>(coefficientForMilliseconds(220.0));
        limiterReleaseCoeff = static_cast<float>(coefficientForMilliseconds(120.0));
        boothSmoothingCoeff = static_cast<float>(1.0 - coefficientForMilliseconds(20.0));
        resetRealtimeState();
        resetMetrics();
    }

    void resetRealtimeState() noexcept {
        microphoneEnvelope = 0.0f;
        duckGainState = 1.0f;
        limiterGainState = 1.0f;
        boothGainState = decibelsToGain(boothGainDb.load(std::memory_order_relaxed));
        currentMicrophoneEnvelope.store(0.0f, std::memory_order_relaxed);
        currentDuckGain.store(1.0f, std::memory_order_relaxed);
        currentLimiterGain.store(1.0f, std::memory_order_relaxed);
        currentBoothGain.store(boothGainState, std::memory_order_relaxed);
    }

    void resetMetrics() noexcept {
        maxInputPeak.store(0.0f, std::memory_order_relaxed);
        maxOutputPeak.store(0.0f, std::memory_order_relaxed);
        maxBoothPeak.store(0.0f, std::memory_order_relaxed);
        maxGainReductionDb.store(0.0f, std::memory_order_relaxed);
    }

    void setMicrophoneEnabled(bool enabled) noexcept {
        microphoneEnabled.store(enabled, std::memory_order_release);
    }
    void setLimiterEnabled(bool enabled) noexcept {
        limiterEnabled.store(enabled, std::memory_order_release);
    }
    void setBoothEnabled(bool enabled) noexcept {
        boothEnabled.store(enabled, std::memory_order_release);
    }
    void setMicrophoneGainDb(float decibels) noexcept {
        if (!std::isfinite(decibels)) decibels = 0.0f;
        microphoneGainDb.store(std::clamp(decibels, -24.0f, 24.0f), std::memory_order_release);
    }
    void setDuckDepthDb(float decibels) noexcept {
        if (!std::isfinite(decibels)) decibels = 0.0f;
        duckDepthDb.store(std::clamp(decibels, 0.0f, 24.0f), std::memory_order_release);
    }
    void setLimiterCeilingDb(float decibels) noexcept {
        if (!std::isfinite(decibels)) decibels = -1.0f;
        limiterCeilingDb.store(std::clamp(decibels, -12.0f, 0.0f), std::memory_order_release);
    }
    void setBoothGainDb(float decibels) noexcept {
        if (!std::isfinite(decibels)) decibels = -6.0f;
        // Booth is attenuation-only in the current baseline so duplicating the
        // protected master cannot create a new post-limiter overload by design.
        boothGainDb.store(std::clamp(decibels, -60.0f, 0.0f), std::memory_order_release);
    }

    [[nodiscard]] bool isMicrophoneEnabled() const noexcept {
        return microphoneEnabled.load(std::memory_order_acquire);
    }
    [[nodiscard]] bool isLimiterEnabled() const noexcept {
        return limiterEnabled.load(std::memory_order_acquire);
    }
    [[nodiscard]] bool isBoothEnabled() const noexcept {
        return boothEnabled.load(std::memory_order_acquire);
    }

    // `microphone` may be null. In that case the mic path is treated as silence
    // even when enabled. `masterLeft/masterRight` must point to the already
    // rendered master outputs. Optional booth pointers receive a post-limiter
    // copy only when booth routing is enabled; when both pointers are provided
    // but booth is disabled they are explicitly cleared. The method performs no
    // allocation, I/O, locking or unbounded work and is safe for the audio callback.
    void process(const float* microphone, float* masterLeft, float* masterRight,
                 int frames, float* boothLeft = nullptr, float* boothRight = nullptr) noexcept {
        if (masterLeft == nullptr || masterRight == nullptr || frames <= 0) return;

        const bool micOn = microphoneEnabled.load(std::memory_order_acquire) && microphone != nullptr;
        const bool limitOn = limiterEnabled.load(std::memory_order_acquire);
        const bool boothPointers = boothLeft != nullptr && boothRight != nullptr;
        const bool boothOn = boothEnabled.load(std::memory_order_acquire) && boothPointers;
        const float micGain = decibelsToGain(microphoneGainDb.load(std::memory_order_acquire));
        const float duckFloor = decibelsToGain(-duckDepthDb.load(std::memory_order_acquire));
        const float ceiling = decibelsToGain(limiterCeilingDb.load(std::memory_order_acquire));
        const float boothTarget = decibelsToGain(boothGainDb.load(std::memory_order_acquire));

        constexpr float duckThreshold = 0.0039810717f; // -48 dBFS
        constexpr float duckFull = 0.0630957344f;      // -24 dBFS
        const float duckSpan = duckFull - duckThreshold;

        float blockInputPeak = 0.0f;
        float blockOutputPeak = 0.0f;
        float blockBoothPeak = 0.0f;
        float blockMaxReduction = 0.0f;

        for (int frame = 0; frame < frames; ++frame) {
            float mic = 0.0f;
            if (micOn) {
                const float raw = microphone[frame];
                mic = std::isfinite(raw) ? raw * micGain : 0.0f;
            }

            const float micMagnitude = std::abs(mic);
            const float envelopeCoeff = micMagnitude > microphoneEnvelope
                ? micAttackCoeff : micReleaseCoeff;
            microphoneEnvelope = envelopeCoeff * microphoneEnvelope
                + (1.0f - envelopeCoeff) * micMagnitude;

            const float activity = std::clamp(
                (microphoneEnvelope - duckThreshold) / duckSpan, 0.0f, 1.0f);
            const float duckTarget = 1.0f - activity * (1.0f - duckFloor);
            // Fast attenuation, slower recovery, independent of the mic envelope
            // release so speech stops sounding like a hard gate.
            if (duckTarget < duckGainState)
                duckGainState = duckTarget;
            else
                duckGainState = micReleaseCoeff * duckGainState
                    + (1.0f - micReleaseCoeff) * duckTarget;

            const float sourceLeft = std::isfinite(masterLeft[frame]) ? masterLeft[frame] : 0.0f;
            const float sourceRight = std::isfinite(masterRight[frame]) ? masterRight[frame] : 0.0f;
            float mixedLeft = sourceLeft * duckGainState + mic;
            float mixedRight = sourceRight * duckGainState + mic;
            if (!std::isfinite(mixedLeft)) mixedLeft = 0.0f;
            if (!std::isfinite(mixedRight)) mixedRight = 0.0f;

            const float inputPeak = std::max(std::abs(mixedLeft), std::abs(mixedRight));
            blockInputPeak = std::max(blockInputPeak, inputPeak);

            if (!limitOn) {
                // A bypass request is an explicit control decision: do not leave
                // residual attenuation from an earlier limiter event.
                limiterGainState = 1.0f;
            } else {
                const float requestedGain = inputPeak > ceiling && inputPeak > 1.0e-12f
                    ? ceiling / inputPeak : 1.0f;
                if (requestedGain < limiterGainState) {
                    // Zero-attack sample-peak protection. This is deliberately not
                    // described as transparent or true-peak limiting.
                    limiterGainState = requestedGain;
                } else {
                    // The release may only rise as far as the current sample is
                    // safe. Without this clamp, a sustained peak can alternate
                    // between attack and release and briefly exceed the ceiling.
                    const float releaseCandidate = 1.0f
                        - (1.0f - limiterGainState) * limiterReleaseCoeff;
                    limiterGainState = std::min(requestedGain, releaseCandidate);
                }
            }

            float outputLeft = mixedLeft * limiterGainState;
            float outputRight = mixedRight * limiterGainState;
            if (!std::isfinite(outputLeft)) outputLeft = 0.0f;
            if (!std::isfinite(outputRight)) outputRight = 0.0f;
            masterLeft[frame] = outputLeft;
            masterRight[frame] = outputRight;

            boothGainState += boothSmoothingCoeff * (boothTarget - boothGainState);
            if (!std::isfinite(boothGainState)) boothGainState = boothTarget;
            if (boothPointers) {
                float boothOutLeft = 0.0f;
                float boothOutRight = 0.0f;
                if (boothOn) {
                    boothOutLeft = outputLeft * boothGainState;
                    boothOutRight = outputRight * boothGainState;
                    if (!std::isfinite(boothOutLeft)) boothOutLeft = 0.0f;
                    if (!std::isfinite(boothOutRight)) boothOutRight = 0.0f;
                }
                boothLeft[frame] = boothOutLeft;
                boothRight[frame] = boothOutRight;
                blockBoothPeak = std::max(blockBoothPeak,
                    std::max(std::abs(boothOutLeft), std::abs(boothOutRight)));
            }

            blockOutputPeak = std::max(blockOutputPeak,
                std::max(std::abs(outputLeft), std::abs(outputRight)));
            const float reduction = limitOn && limiterGainState < 1.0f
                ? -20.0f * std::log10(std::max(limiterGainState, 1.0e-12f)) : 0.0f;
            blockMaxReduction = std::max(blockMaxReduction, reduction);
        }

        currentMicrophoneEnvelope.store(microphoneEnvelope, std::memory_order_relaxed);
        currentDuckGain.store(duckGainState, std::memory_order_relaxed);
        currentLimiterGain.store(limiterGainState, std::memory_order_relaxed);
        currentBoothGain.store(boothGainState, std::memory_order_relaxed);
        storeMaximum(maxInputPeak, blockInputPeak);
        storeMaximum(maxOutputPeak, blockOutputPeak);
        storeMaximum(maxBoothPeak, blockBoothPeak);
        storeMaximum(maxGainReductionDb, blockMaxReduction);
    }

    [[nodiscard]] Snapshot snapshot() const noexcept {
        Snapshot result;
        result.microphoneEnabled = microphoneEnabled.load(std::memory_order_acquire);
        result.limiterEnabled = limiterEnabled.load(std::memory_order_acquire);
        result.boothEnabled = boothEnabled.load(std::memory_order_acquire);
        result.microphoneEnvelope = currentMicrophoneEnvelope.load(std::memory_order_relaxed);
        result.duckGain = currentDuckGain.load(std::memory_order_relaxed);
        result.limiterGain = currentLimiterGain.load(std::memory_order_relaxed);
        result.boothGain = currentBoothGain.load(std::memory_order_relaxed);
        result.maxInputPeak = maxInputPeak.load(std::memory_order_relaxed);
        result.maxOutputPeak = maxOutputPeak.load(std::memory_order_relaxed);
        result.maxBoothPeak = maxBoothPeak.load(std::memory_order_relaxed);
        result.maxGainReductionDb = maxGainReductionDb.load(std::memory_order_relaxed);
        return result;
    }

private:
    [[nodiscard]] double coefficientForMilliseconds(double milliseconds) const noexcept {
        const double samples = std::max(1.0, sampleRate * milliseconds * 0.001);
        return std::exp(-1.0 / samples);
    }

    [[nodiscard]] static float decibelsToGain(float decibels) noexcept {
        if (!std::isfinite(decibels)) return 1.0f;
        return std::pow(10.0f, decibels / 20.0f);
    }

    static void storeMaximum(std::atomic<float>& destination, float candidate) noexcept {
        if (!std::isfinite(candidate) || candidate <= 0.0f) return;
        float current = destination.load(std::memory_order_relaxed);
        while (candidate > current
               && !destination.compare_exchange_weak(current, candidate,
                    std::memory_order_relaxed, std::memory_order_relaxed)) {}
    }

    double sampleRate = 48000.0;
    float micAttackCoeff = 0.0f;
    float micReleaseCoeff = 0.0f;
    float limiterReleaseCoeff = 0.0f;
    float boothSmoothingCoeff = 0.0f;
    float microphoneEnvelope = 0.0f;
    float duckGainState = 1.0f;
    float limiterGainState = 1.0f;
    float boothGainState = 1.0f;

    std::atomic<bool> microphoneEnabled{false};
    std::atomic<bool> limiterEnabled{true};
    std::atomic<bool> boothEnabled{false};
    std::atomic<float> microphoneGainDb{0.0f};
    std::atomic<float> duckDepthDb{12.0f};
    std::atomic<float> limiterCeilingDb{-1.0f};
    std::atomic<float> boothGainDb{-6.0f};

    std::atomic<float> currentMicrophoneEnvelope{0.0f};
    std::atomic<float> currentDuckGain{1.0f};
    std::atomic<float> currentLimiterGain{1.0f};
    std::atomic<float> currentBoothGain{1.0f};
    std::atomic<float> maxInputPeak{0.0f};
    std::atomic<float> maxOutputPeak{0.0f};
    std::atomic<float> maxBoothPeak{0.0f};
    std::atomic<float> maxGainReductionDb{0.0f};
};

} // namespace broke
