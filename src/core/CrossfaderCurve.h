// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace broke {

enum class CrossfaderCurve : std::uint8_t {
    constantPower = 0,
    linear = 1,
    fastCut = 2,
};

struct CrossfaderGains final {
    float left = 1.0f;
    float right = 0.0f;
};

[[nodiscard]] inline CrossfaderCurve crossfaderCurveFromRaw(std::uint8_t raw) noexcept {
    switch (raw) {
        case static_cast<std::uint8_t>(CrossfaderCurve::constantPower):
            return CrossfaderCurve::constantPower;
        case static_cast<std::uint8_t>(CrossfaderCurve::linear):
            return CrossfaderCurve::linear;
        case static_cast<std::uint8_t>(CrossfaderCurve::fastCut):
            return CrossfaderCurve::fastCut;
        default:
            return CrossfaderCurve::constantPower;
    }
}

// Pure JUCE-independent crossfader law. The callback still smooths both the
// physical fader position and the resulting gain pair, so changing curve mode
// cannot introduce an unsmoothed gain discontinuity. Fast Cut intentionally
// narrows the blend region while keeping a continuous cubic transition.
[[nodiscard]] inline CrossfaderGains crossfaderGains(float position,
                                                      CrossfaderCurve curve) noexcept {
    const float x = std::isfinite(position) ? std::clamp(position, 0.0f, 1.0f) : 0.5f;
    switch (curve) {
        case CrossfaderCurve::linear:
            return {1.0f - x, x};
        case CrossfaderCurve::fastCut: {
            float t = std::clamp((x - 0.35f) / 0.30f, 0.0f, 1.0f);
            t = t * t * (3.0f - 2.0f * t);
            return {1.0f - t, t};
        }
        case CrossfaderCurve::constantPower:
        default:
            return {
                std::cos(x * std::numbers::pi_v<float> * 0.5f),
                std::sin(x * std::numbers::pi_v<float> * 0.5f),
            };
    }
}

} // namespace broke
