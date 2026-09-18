// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>
#include <juce_audio_formats/juce_audio_formats.h>
#include "core/Engine.h"

struct DecodeResult {
    std::unique_ptr<broke::Clip> clip;
    std::vector<float> peaks;
    juce::String name, error;
};

// Internal decoder policy. Production callers use the defaults. Tests may lower
// the streaming threshold and throttle background refill to exercise the same
// read-ahead implementation deterministically without allocating huge fixtures.
struct DecodeOptions final {
    std::int64_t streamingThresholdBytes = 64LL * 1024 * 1024;
    int readAheadDelayMs = 0;
};

// A worker-only decoder. No file I/O is performed by the audio callback.
DecodeResult decodeTrack(const juce::File& file, const std::atomic<bool>& cancelled,
                         const DecodeOptions& options = {});
