// SPDX-License-Identifier: AGPL-3.0-only
#include "Decoder.h"
#include "WaveformCache.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace {
constexpr int previewBuckets = 512;
constexpr int previewWindow = 2048;
constexpr int previewSequentialBlock = 65536;
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

bool buildSequentialPeaks(juce::AudioFormatReader& reader, const std::atomic<bool>& cancelled,
                          std::vector<float>& peaks) {
    peaks.assign(previewBuckets, 0.0f);
    juce::AudioBuffer<float> scratch(2, previewSequentialBlock);
    const auto totalFrames = reader.lengthInSamples;
    for (juce::int64 offset = 0; offset < totalFrames;) {
        if (cancelled.load()) return false;
        const int count = static_cast<int>(std::min<juce::int64>(
            previewSequentialBlock, totalFrames - offset));
        scratch.clear();
        float* channels[] {scratch.getWritePointer(0), scratch.getWritePointer(1)};
        if (!reader.read(channels, 2, offset, count)) return false;
        if (reader.numChannels == 1)
            std::copy_n(scratch.getReadPointer(0), count, scratch.getWritePointer(1));

        for (int i = 0; i < count; ++i) {
            const auto frame = offset + static_cast<juce::int64>(i);
            const auto bucket = std::min<std::int64_t>(
                previewBuckets - 1,
                frame * static_cast<juce::int64_t>(previewBuckets) / totalFrames);
            const float left = std::isfinite(channels[0][i]) ? channels[0][i] : 0.0f;
            const float right = std::isfinite(channels[1][i]) ? channels[1][i] : 0.0f;
            peaks[static_cast<std::size_t>(bucket)] = std::max(
                peaks[static_cast<std::size_t>(bucket)],
                std::min(1.0f, std::max(std::abs(left), std::abs(right))));
        }
        offset += count;
    }
    return true;
}

class StreamingTrack final : public juce::Thread {
public:
    StreamingTrack(std::unique_ptr<juce::AudioFormatReader> input,
                   int sourceChannels,
                   std::shared_ptr<broke::StreamCache> cacheIn,
                   int readAheadDelayMsIn)
        : juce::Thread("BrokeDJ read-ahead"), reader(std::move(input)),
          channels(sourceChannels), cache(std::move(cacheIn)),
          readAheadDelayMs(std::clamp(readAheadDelayMsIn, 0, 1000)),
          scratch(2, static_cast<int>(broke::StreamCache::chunkFrames)) {}

    ~StreamingTrack() override {
        signalThreadShouldExit();
        notify();
        stopThread(3000);
    }

    bool prime(const std::atomic<bool>& cancelled) {
        cache->request(0);
        for (int i = 0; i < primeChunks; ++i) {
            if (cancelled.load()) return false;
            if (fillChunk(i, false, -1) == FillResult::failed) return false;
        }
        return true;
    }

    void run() override {
        const auto totalChunks = (reader->lengthInSamples
            + static_cast<juce::int64>(broke::StreamCache::chunkFrames) - 1)
            / static_cast<juce::int64>(broke::StreamCache::chunkFrames);
        std::int64_t lastRequestedChunk = -1;
        std::int64_t prefetchDirection = 1;
        while (!threadShouldExit()) {
            const auto requested = cache->requestedFrame();
            const auto centre = requested / static_cast<std::int64_t>(broke::StreamCache::chunkFrames);
            if (lastRequestedChunk >= 0 && centre != lastRequestedChunk)
                prefetchDirection = centre < lastRequestedChunk ? -1 : 1;
            lastRequestedChunk = centre;

            bool didWork = false;
            bool readFailed = false;
            bool requestChanged = false;

            const auto requestedChunkChanged = [this, centre] {
                return cache->requestedFrame() / static_cast<std::int64_t>(broke::StreamCache::chunkFrames)
                    != centre;
            };
            const auto tryFill = [this, totalChunks, centre, &didWork, &readFailed](std::int64_t chunk) {
                if (chunk < 0 || chunk >= totalChunks || cache->hasChunk(chunk)) return;
                switch (fillChunk(chunk, true, centre)) {
                    case FillResult::filled:
                        didWork = true;
                        break;
                    case FillResult::failed:
                        readFailed = !threadShouldExit();
                        break;
                    case FillResult::noWork:
                        break;
                }
            };
            const auto stillCurrent = [&] {
                if (requestedChunkChanged()) {
                    requestChanged = true;
                    return false;
                }
                return true;
            };

            // Recovery always loads the requested chunk first, then both immediate
            // neighbours because interpolation may need source samples on either
            // side. Deeper read-ahead follows the observed transport direction.
            // This keeps ordinary forward playback unchanged while allowing a
            // sustained reverse/slip cursor to build cache behind the playhead.
            // A one-off backward seek can briefly bias prefetch backward, but the
            // exact chunk plus both neighbours remain first priority and the next
            // changed request re-establishes the actual travel direction.
            tryFill(centre);
            if (!threadShouldExit() && !readFailed && stillCurrent()) tryFill(centre - 1);
            if (!threadShouldExit() && !readFailed && stillCurrent()) tryFill(centre + 1);
            for (int offset = 2; offset < readAheadChunks && !threadShouldExit() && !readFailed; ++offset) {
                if (!stillCurrent()) break;
                tryFill(centre + prefetchDirection * static_cast<std::int64_t>(offset));
            }

            if (requestChanged) continue;
            if (readFailed) wait(20);
            else if (!didWork) wait(4);
        }
    }

private:
    enum class FillResult {
        noWork,
        filled,
        failed
    };

    FillResult fillChunk(std::int64_t chunk, bool throttled,
                         std::int64_t expectedRequestedChunk) {
        const auto start = chunk * static_cast<std::int64_t>(broke::StreamCache::chunkFrames);
        if (start < 0 || start >= reader->lengthInSamples) return FillResult::noWork;

        const auto requestStillCurrent = [this, expectedRequestedChunk] {
            if (expectedRequestedChunk < 0) return true;
            const auto current = cache->requestedFrame()
                / static_cast<std::int64_t>(broke::StreamCache::chunkFrames);
            return current == expectedRequestedChunk;
        };
        if (!requestStillCurrent()) return FillResult::noWork;

        if (throttled && readAheadDelayMs > 0) {
            int remaining = readAheadDelayMs;
            while (remaining > 0 && !threadShouldExit()) {
                if (!requestStillCurrent()) return FillResult::noWork;
                const int slice = std::min(remaining, 2);
                juce::Thread::sleep(slice);
                remaining -= slice;
            }
            if (threadShouldExit()) return FillResult::failed;
            if (!requestStillCurrent()) return FillResult::noWork;
        }

        // The request check immediately before reader->read prevents a seek or
        // rapid reverse-direction change from starting a decoder read for a
        // region that is already stale. Once reader->read begins it is allowed
        // to complete and publish valid decoded data; interrupting third-party
        // codec internals is deliberately outside this worker's contract.
        if (!requestStillCurrent()) return FillResult::noWork;
        const int count = static_cast<int>(std::min<juce::int64>(
            static_cast<juce::int64>(broke::StreamCache::chunkFrames),
            reader->lengthInSamples - start));
        scratch.clear();
        float* destinations[] {scratch.getWritePointer(0), scratch.getWritePointer(1)};
        if (!reader->read(destinations, 2, start, count)) return FillResult::failed;
        for (int i = 0; i < count; ++i) {
            if (!std::isfinite(destinations[0][i])) destinations[0][i] = 0.0f;
            if (!std::isfinite(destinations[1][i])) destinations[1][i] = 0.0f;
        }
        if (channels == 1)
            std::copy_n(scratch.getReadPointer(0), count, scratch.getWritePointer(1));
        cache->publishChunk(chunk, scratch.getReadPointer(0), scratch.getReadPointer(1),
                            static_cast<std::size_t>(count));
        return FillResult::filled;
    }

    std::unique_ptr<juce::AudioFormatReader> reader;
    int channels = 2;
    std::shared_ptr<broke::StreamCache> cache;
    int readAheadDelayMs = 0;
    juce::AudioBuffer<float> scratch;
};
}

DecodeResult decodeTrack(const juce::File& file, const std::atomic<bool>& cancelled,
                         const DecodeOptions& options) {
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

        std::unique_ptr<WaveformPreviewCache> waveformCache;
        if (options.enableWaveformCache) {
            auto cacheRoot = options.waveformCacheRoot;
            if (cacheRoot.getFullPathName().isEmpty())
                cacheRoot = WaveformPreviewCache::defaultRoot();
            waveformCache = std::make_unique<WaveformPreviewCache>(cacheRoot);
            result.waveformCacheHit = waveformCache->load(file, result.peaks);
        }
        const auto persistWaveform = [&] {
            if (waveformCache != nullptr && !result.waveformCacheHit
                && !cancelled.load(std::memory_order_acquire)) {
                static_cast<void>(waveformCache->store(file, result.peaks));
            }
        };

        constexpr std::int64_t bytesPerStereoFrame = static_cast<std::int64_t>(sizeof(float) * 2);
        const auto thresholdBytes = std::max<std::int64_t>(1, options.streamingThresholdBytes);
        const bool useStreaming = reader->lengthInSamples > thresholdBytes / bytesPerStereoFrame;

        if (useStreaming) {
            if (!result.waveformCacheHit
                && !buildSparsePeaks(*reader, cancelled, result.peaks)) {
                if (cancelled.load()) {
                    result.error = "Import cancelled.";
                    return result;
                }

                // Some compressed readers cannot reliably satisfy hundreds of tiny,
                // non-monotonic preview seeks. Fall back to one bounded-memory
                // sequential pass on a fresh decoder instead of rejecting a track
                // that is otherwise playable. This work remains off the audio thread.
                reader.reset(formats.createReaderFor(file));
                if (!reader || !validReader(*reader)
                    || !buildSequentialPeaks(*reader, cancelled, result.peaks)) {
                    result.error = cancelled.load()
                        ? "Import cancelled."
                        : "Read error while building waveform preview.";
                    return result;
                }
            }
            if (cancelled.load()) {
                result.error = "Import cancelled.";
                return result;
            }
            persistWaveform();

            // Preview extraction may leave a compressed decoder at an arbitrary
            // position/state. Playback always gets a fresh reader, isolating cache
            // priming and future read-ahead from waveform-generation behavior.
            reader.reset(formats.createReaderFor(file));
            if (!reader || !validReader(*reader)) {
                result.error = "Cannot reopen the audio decoder for streaming playback.";
                return result;
            }

            const auto sourceRate = reader->sampleRate;
            const auto sourceFrames = reader->lengthInSamples;
            const auto sourceChannels = static_cast<int>(reader->numChannels);
            auto cache = std::make_shared<broke::StreamCache>(sourceFrames);
            auto source = std::make_shared<StreamingTrack>(std::move(reader), sourceChannels, cache,
                                                           options.readAheadDelayMs);
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
        if (!result.waveformCacheHit)
            result.peaks.assign(previewBuckets, 0.0f);
        for (std::size_t i = 0; i < length; ++i) {
            if (!std::isfinite(clip->left[i])) clip->left[i] = 0.0f;
            if (!std::isfinite(clip->right[i])) clip->right[i] = 0.0f;
            if (!result.waveformCacheHit) {
                const auto bucket = std::min<std::size_t>(previewBuckets - 1, i * previewBuckets / length);
                result.peaks[bucket] = std::max(result.peaks[bucket],
                    std::min(1.0f, std::max(std::abs(clip->left[i]), std::abs(clip->right[i]))));
            }
        }
        persistWaveform();
        result.clip = std::move(clip);
    } catch (const std::exception& error) {
        result.error = "Import failed: " + juce::String(error.what());
    } catch (...) {
        result.error = "Import failed with an unknown decoder error.";
    }
    return result;
}
