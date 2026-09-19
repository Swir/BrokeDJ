// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace broke {

enum class KeyMode : std::uint8_t {
    unknown = 0,
    major = 1,
    minor = 2,
};

struct MusicalKeyResult final {
    bool valid = false;
    int tonic = -1; // C=0 ... B=11
    KeyMode mode = KeyMode::unknown;
    double confidence = 0.0;
    double analyzedSeconds = 0.0;
    bool truncated = false;
};

struct KeyAnalysisOptions final {
    double targetRate = 6000.0;
    double maxAnalysisSeconds = 180.0;
    std::size_t windowSize = 2048;
};

// Streaming/offline musical-key analysis. Input is decimated through a bounded
// box average, then analyzed in fixed windows with a 48-note Goertzel chroma
// bank. The resulting chroma is compared against major/minor key profiles. This
// is worker-only analysis, not realtime-safe, and intentionally rejects flat
// noise-like spectra and single-tone material instead of inventing a key.
class KeyAnalysisAccumulator final {
public:
    KeyAnalysisAccumulator(double sampleRate, KeyAnalysisOptions options = {});

    [[nodiscard]] bool configured() const noexcept { return configuredFlag; }
    void push(const float* left, const float* right, std::size_t frames);
    [[nodiscard]] MusicalKeyResult finish();

private:
    void emitDecimatedSample();
    void processWindow();

    KeyAnalysisOptions options;
    double sourceRate = 0.0;
    double analysisRate = 0.0;
    std::size_t decimationStride = 0;
    std::size_t decimationCount = 0;
    double decimationSum = 0.0;
    std::size_t maxDecimatedSamples = 0;
    std::uint64_t acceptedInputSamples = 0;
    std::size_t emittedSamples = 0;
    std::size_t windowsProcessed = 0;
    bool configuredFlag = false;
    bool truncatedFlag = false;
    bool finishedFlag = false;
    std::vector<float> window;
    std::array<double, 12> chroma{};
};

[[nodiscard]] MusicalKeyResult analyzeMusicalKey(const float* left,
                                                 const float* right,
                                                 std::size_t frames,
                                                 double sampleRate,
                                                 KeyAnalysisOptions options = {});
[[nodiscard]] const char* keyName(int tonic) noexcept;

} // namespace broke
