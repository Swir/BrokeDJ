// SPDX-License-Identifier: AGPL-3.0-only
#include "Decoder.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace {
constexpr juce::int64 inMemoryBytes = 64LL * 1024 * 1024;
constexpr int previewBuckets = 512;
constexpr int previewWindow = 2048;
constexpr int primeChunks = 8;
constexpr int readAheadChunks = 12;

bool validReader(const juce::AudioFormatReader& reader) {
    return reader.lengthInSamples > 0
        && reader.numChannels >= 1 && reader.numChannels <= 2
        && std::isfinite(reader.sampleRate)
        && reader.sampleRate >= 8000.0 && reader.sampleRate <= 384000.0;
}

bool buildSparsePeaks(juce::AudioFormatReader& reader, const std::atomic<bool>& cancelled,
                      std::vector<float>& peaks) {
    peaks.assign(previewBuckets, 0.0f);
    juce::AudioBuffer<float> scratch(2, previewWindow);
    for (int bucket = 0; bucket < previewBuckets; ++bucket) {
        if (cancelled.load()) return false;
        const auto start = (reader.lengthInSamples - 1) * static_cast<juce::int64>(bucket)
            / static_cast<juce::int64>(previewBuckets);
        const int count = static_cast<int>(std::min<juce::int64>(
            previewWindow, reader.lengthInSamples - start));
        if (count <= 0) continue;
        scratch.clear();
        float* channels[] {scratch.getWritePointer(0), scratch.getWritePointer(1)};
        if (!reader.read(channels, 2, start, count)) return false;
        if (reader.numChannels == 1)
            std::copy_n(scratch.getReadPointer(0), count, scratch.getWritePointer(1));
        float peak = 0.0f;
        for (int i = 0; i < count; ++i) {
            const float left = std::isfinite(channels[0][i]) ? channels[0][i] : 0.0f;
            const float right = std::isfinite(channels[1][i]) ? channels[1][i] : 0.0f;
            peak = std::max(peak, std::min(1.0f, std::max(std::abs(left), std::abs(right))));
        }
        peaks[static_cast<std::size_t>(bucket)] = peak;
    }
    return true;
}

class StreamingTrack final : public juce::Thread {
public:
    StreamingTrack(std::unique_ptr<juce::AudioFormatReader> input,
                   int sourceChannels,
                   std::shared_ptr<broke::StreamCache> cacheIn)
        : juce::Thread("BrokeDJ read-ahead"), reader(std::move(input)),
          channels(sourceChannels), cache(std::move(cacheIn)),
          scratch(2, static_cast<int>(broke::StreamCache::chunkFrames)) {}

    ~StreamingTrack() override {
        signalThreadShouldExit();
        notify();
        stopThread(3000);
    }

    bool prime(const std::atomic<bool>& cancelled) {
        cache->request(0);
        for (int i = 0; i < primeChunks; ++i) {
            if (cancelled.load() || !fillChunk(i)) return !cancelled.load();
        }
        return true;
    }

    void run() override {
        while (!threadShouldExit()) {
            const auto requested = cache->requestedFrame();
            const auto centre = requested / static_cast<std::int64_t>(broke::StreamCache::chunkFrames);
            bool didWork = false;
            for (int offset = -1; offset < readAheadChunks && !threadShouldExit(); ++offset) {
                const auto chunk = centre + offset;
                if (chunk < 0 || cache->hasChunk(chunk)) continue;
                if (fillChunk(chunk)) didWork = true;
            }
            if (!didWork) wait(4);
        }
    }

private:
    bool fillChunk(std::int64_t chunk) {
        const auto start = chunk * static_cast<std::int64_t>(broke::StreamCache::chunkFrames);
        if (start < 0 || start >= reader->lengthInSamples) return true;
        const int count = static_cast<int>(std::min<juce::int64>(
            static_cast<juce::int64>(broke::StreamCache::chunkFrames),
            reader->lengthInSamples - start));
        scratch.clear();
        float* destinations[] {scratch.getWritePointer(0), scratch.getWritePointer(1)};
        if (!reader->read(destinations, 2, start, count)) return false;
        if (channels == 1)
            std::copy_n(scratch.getReadPointer(0), count, scratch.getWritePointer(1));
        cache->publishChunk(chunk, scratch.getReadPointer(0), scratch.getReadPointer(1),
                            static_cast<std::size_t>(count));
        return true;
    }

    std::unique_ptr<juce::AudioFormatReader> reader;
    int channels = 2;
    std::shared_ptr<broke::StreamCache> cache;
    juce::AudioBuffer<float> scratch;
};
}

DecodeResult decodeTrack(const juce::File& file, const std::atomic<bool>& cancelled) {
    DecodeResult result;
    result.name = file.getFileName();
    try {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (!reader) {
            result.error = "Cannot decode this file. Use WAV, AIFF, FLAC, OGG or MP3.";
            return result;
        }
        if (!validReader(*reader)) {
            result.error = "Unsupported track: use mono/stereo audio at 8-384 kHz.";
            return result;
        }

        constexpr juce::int64 bytesPerStereoFrame = static_cast<juce::int64>(sizeof(float) * 2);
        const bool useStreaming = reader->lengthInSamples > inMemoryBytes / bytesPerStereoFrame;

        if (useStreaming) {
            const auto sourceRate = reader->sampleRate;
            const auto sourceFrames = reader->lengthInSamples;
            const auto sourceChannels = static_cast<int>(reader->numChannels);
            if (!buildSparsePeaks(*reader, cancelled, result.peaks)) {
                result.error = cancelled.load() ? "Import cancelled." : "Read error while building waveform preview.";
                return result;
            }
            auto cache = std::make_shared<broke::StreamCache>(sourceFrames);
            auto source = std::make_shared<StreamingTrack>(std::move(reader), sourceChannels, cache);
            if (!source->prime(cancelled)) {
                result.error = cancelled.load() ? "Import cancelled." : "Read error while priming streaming cache.";
                return result;
            }
            if (!source->startThread(juce::Thread::Priority::background)) {
                result.error = "Cannot start the background streaming reader.";
                return result;
            }
            auto clip = std::make_unique<broke::Clip>();
            clip->sampleRate = sourceRate;
            clip->frameCount = sourceFrames;
            clip->stream = cache;
            clip->sourceOwner = source;
            result.clip = std::move(clip);
            return result;
        }

        auto clip = std::make_unique<broke::Clip>();
        clip->sampleRate = reader->sampleRate;
        const auto length = static_cast<std::size_t>(reader->lengthInSamples);
        clip->left.resize(length);
        clip->right.resize(length);
        for (std::size_t offset = 0; offset < length;) {
            if (cancelled.load()) {
                result.error = "Import cancelled.";
                return result;
            }
            const int count = static_cast<int>(std::min<std::size_t>(65536, length - offset));
            float* channels[] {clip->left.data() + offset, clip->right.data() + offset};
            if (!reader->read(channels, 2, static_cast<juce::int64>(offset), count)) {
                result.error = "Read error: the audio file may be damaged.";
                return result;
            }
            offset += static_cast<std::size_t>(count);
        }
        if (reader->numChannels == 1)
            std::copy(clip->left.begin(), clip->left.end(), clip->right.begin());
        result.peaks.assign(previewBuckets, 0.0f);
        for (std::size_t i = 0; i < length; ++i) {
            if (!std::isfinite(clip->left[i])) clip->left[i] = 0.0f;
            if (!std::isfinite(clip->right[i])) clip->right[i] = 0.0f;
            const auto bucket = std::min<std::size_t>(previewBuckets - 1, i * previewBuckets / length);
            result.peaks[bucket] = std::max(result.peaks[bucket],
                std::min(1.0f, std::max(std::abs(clip->left[i]), std::abs(clip->right[i]))));
        }
        result.clip = std::move(clip);
    } catch (const std::exception& error) {
        result.error = "Import failed: " + juce::String(error.what());
    } catch (...) {
        result.error = "Import failed with an unknown decoder error.";
    }
    return result;
}
