// SPDX-License-Identifier: AGPL-3.0-only
#include "Decoder.h"
#include <algorithm>
#include <cmath>
#include <limits>
DecodeResult decodeTrack(const juce::File& file, const std::atomic<bool>& cancelled) {
    DecodeResult result;
    result.name = file.getFileName();
    try {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (!reader) { result.error = "Cannot decode this file. Use WAV, AIFF, FLAC, OGG or MP3."; return result; }
        constexpr juce::int64 maximumBytes = 256LL * 1024 * 1024;
        if (reader->lengthInSamples <= 0 || reader->lengthInSamples > maximumBytes / 8
            || reader->numChannels < 1 || reader->numChannels > 2
            || !std::isfinite(reader->sampleRate) || reader->sampleRate < 8000 || reader->sampleRate > 384000) {
            result.error = "Unsupported track: use mono/stereo, 8-384 kHz and at most 256 MiB decoded stereo audio.";
            return result;
        }
        auto clip = std::make_unique<broke::Clip>();
        clip->sampleRate = reader->sampleRate;
        const auto length = static_cast<std::size_t>(reader->lengthInSamples);
        clip->left.resize(length); clip->right.resize(length);
        for (std::size_t offset = 0; offset < length;) {
            if (cancelled.load()) { result.error = "Import cancelled."; return result; }
            const int count = static_cast<int>(std::min<std::size_t>(65536, length - offset));
            float* channels[] {clip->left.data() + offset, clip->right.data() + offset};
            if (!reader->read(channels, 2, static_cast<juce::int64>(offset), count)) {
                result.error = "Read error: the audio file may be damaged."; return result;
            }
            offset += static_cast<std::size_t>(count);
        }
        if (reader->numChannels == 1) std::copy(clip->left.begin(), clip->left.end(), clip->right.begin());
        result.peaks.assign(512, 0.0f);
        for (std::size_t i = 0; i < length; ++i) {
            if (!std::isfinite(clip->left[i])) clip->left[i] = 0.0f;
            if (!std::isfinite(clip->right[i])) clip->right[i] = 0.0f;
            const auto bucket = std::min<std::size_t>(511, i * 512 / length);
            result.peaks[bucket] = std::max(result.peaks[bucket], std::min(1.0f, std::max(std::abs(clip->left[i]), std::abs(clip->right[i]))));
        }
        result.clip = std::move(clip);
    } catch (const std::exception& error) {
        result.error = "Import failed: " + juce::String(error.what());
    } catch (...) { result.error = "Import failed with an unknown decoder error."; }
    return result;
}
