// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <JuceHeader.h>
#include "core/Engine.h"
struct DecodeResult {
    std::unique_ptr<broke::Clip> clip;
    std::vector<float> peaks;
    juce::String name, error;
};
// A worker-only decoder. No file I/O is performed by the audio callback.
DecodeResult decodeTrack(const juce::File& file, const std::atomic<bool>& cancelled);
