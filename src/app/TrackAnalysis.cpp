// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#include "TrackAnalysis.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <utility>

namespace {
constexpr int cacheSchema = 1;
constexpr std::int64_t maxCacheBytes = 64 * 1024;
constexpr int maxCachedSegments = 128;

bool validReader(const juce::AudioFormatReader& reader) {
    return reader.lengthInSamples > 0
        && reader.numChannels >= 1 && reader.numChannels <= 2
        && std::isfinite(reader.sampleRate)
        && reader.sampleRate >= 8000.0 && reader.sampleRate <= 384000.0;
}

std::uint64_t fnv1a64(const juce::String& text) {
    constexpr std::uint64_t offset = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t hash = offset;
    const auto utf8 = text.toUTF8();
    for (const char* p = utf8.getAddress(); p != nullptr && *p != '\0'; ++p) {
        hash ^= static_cast<unsigned char>(*p);
        hash *= prime;
    }
    return hash;
}

juce::String hex64(std::uint64_t value) {
    std::ostringstream stream;
    stream << std::hex << std::setw(16) << std::setfill('0') << value;
    return juce::String(stream.str());
}

bool finiteRange(double value, double minimum, double maximum) {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}

juce::StringPairArray parseFields(const juce::String& text) {
    juce::StringPairArray fields;
    juce::StringArray lines;
    lines.addLines(text);
    for (const auto& line : lines) {
        const int separator = line.indexOfChar('=');
        if (separator <= 0) continue;
        fields.set(line.substring(0, separator).trim(), line.substring(separator + 1).trim());
    }
    return fields;
}
}

TrackAnalysisCache::TrackAnalysisCache(juce::File rootIn) : root(std::move(rootIn)) {}

juce::File TrackAnalysisCache::defaultRoot() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("BrokeDJ")
        .getChildFile("analysis-v1");
}

juce::File TrackAnalysisCache::cacheFileFor(const juce::File& source) const {
    // Hash the local path only for the on-disk key. Size and mtime are stored as
    // validation fields so a changed file replaces its prior cache record rather
    // than accumulating one cache file per edit. The path itself is never stored.
    return root.getChildFile(hex64(fnv1a64(source.getFullPathName())) + ".analysis");
}

bool TrackAnalysisCache::load(const juce::File& source,
                              broke::BeatAnalysisResult& result) const {
    result = {};
    if (!source.existsAsFile()) return false;
    const auto cached = cacheFileFor(source);
    if (!cached.existsAsFile() || cached.getSize() <= 0 || cached.getSize() > maxCacheBytes)
        return false;

    const auto fields = parseFields(cached.loadFileAsString());
    const juce::String empty;
    if (fields.getValue("schema", empty).getIntValue() != cacheSchema
        || fields.getValue("fileSize", empty).getLargeIntValue() != source.getSize()
        || fields.getValue("modifiedMs", empty).getLargeIntValue()
            != source.getLastModificationTime().toMilliseconds()
        || fields.getValue("valid", empty) != "1") {
        return false;
    }

    const double bpm = fields.getValue("bpm", empty).getDoubleValue();
    const double beatZero = fields.getValue("beatZeroSeconds", empty).getDoubleValue();
    const double confidence = fields.getValue("confidence", empty).getDoubleValue();
    const double analyzedSeconds = fields.getValue("analyzedSeconds", empty).getDoubleValue();
    const int segmentCount = fields.getValue("segmentCount", empty).getIntValue();
    if (!finiteRange(bpm, 30.0, 300.0)
        || !finiteRange(beatZero, 0.0, 24.0 * 60.0 * 60.0)
        || !finiteRange(confidence, 0.0, 1.0)
        || !finiteRange(analyzedSeconds, 0.0, 4.0 * 60.0 * 60.0)
        || segmentCount <= 0 || segmentCount > maxCachedSegments) {
        return false;
    }

    broke::BeatAnalysisResult decoded;
    decoded.valid = true;
    decoded.bpm = bpm;
    decoded.beatZeroSeconds = beatZero;
    decoded.confidence = confidence;
    decoded.analyzedSeconds = analyzedSeconds;
    decoded.truncated = fields.getValue("truncated", empty) == "1";
    decoded.segments.reserve(static_cast<std::size_t>(segmentCount));
    for (int i = 0; i < segmentCount; ++i) {
        const auto suffix = juce::String(i);
        const double start = fields.getValue("segment" + suffix + "Start", empty).getDoubleValue();
        const double segmentBpm = fields.getValue("segment" + suffix + "Bpm", empty).getDoubleValue();
        if (!finiteRange(start, 0.0, 24.0 * 60.0 * 60.0)
            || !finiteRange(segmentBpm, 30.0, 300.0)) {
            return false;
        }
        decoded.segments.push_back({start, segmentBpm});
    }
    result = std::move(decoded);
    return true;
}

bool TrackAnalysisCache::store(const juce::File& source,
                               const broke::BeatAnalysisResult& result) const {
    if (!source.existsAsFile() || !result.valid
        || !finiteRange(result.bpm, 30.0, 300.0)
        || !finiteRange(result.beatZeroSeconds, 0.0, 24.0 * 60.0 * 60.0)
        || !finiteRange(result.confidence, 0.0, 1.0)
        || !finiteRange(result.analyzedSeconds, 0.0, 4.0 * 60.0 * 60.0)
        || result.segments.empty()
        || result.segments.size() > static_cast<std::size_t>(maxCachedSegments)) {
        return false;
    }
    for (const auto& segment : result.segments) {
        if (!finiteRange(segment.startSeconds, 0.0, 24.0 * 60.0 * 60.0)
            || !finiteRange(segment.bpm, 30.0, 300.0)) return false;
    }
    if (!root.isDirectory() && !root.createDirectory()) return false;

    juce::String payload;
    payload << "schema=" << cacheSchema << "\n";
    payload << "fileSize=" << source.getSize() << "\n";
    payload << "modifiedMs=" << source.getLastModificationTime().toMilliseconds() << "\n";
    payload << "valid=1\n";
    payload << "bpm=" << juce::String(result.bpm, 12) << "\n";
    payload << "beatZeroSeconds=" << juce::String(result.beatZeroSeconds, 12) << "\n";
    payload << "confidence=" << juce::String(result.confidence, 12) << "\n";
    payload << "analyzedSeconds=" << juce::String(result.analyzedSeconds, 12) << "\n";
    payload << "truncated=" << (result.truncated ? 1 : 0) << "\n";
    payload << "segmentCount=" << static_cast<int>(result.segments.size()) << "\n";
    for (std::size_t i = 0; i < result.segments.size(); ++i) {
        payload << "segment" << static_cast<int>(i) << "Start="
                << juce::String(result.segments[i].startSeconds, 12) << "\n";
        payload << "segment" << static_cast<int>(i) << "Bpm="
                << juce::String(result.segments[i].bpm, 12) << "\n";
    }

    const auto target = cacheFileFor(source);
    juce::TemporaryFile temporary(target);
    if (!temporary.getFile().replaceWithText(payload, false, false, "\n")) return false;
    return temporary.overwriteTargetFileWithTemporary();
}

TrackRhythmAnalysis analyzeTrackRhythm(const juce::File& file,
                                       const std::atomic<bool>& cancelled,
                                       const TrackAnalysisOptions& options) {
    TrackRhythmAnalysis result;
    if (cancelled.load()) {
        result.error = "Analysis cancelled.";
        return result;
    }
    if (!file.existsAsFile()) {
        result.error = "Analysis source is missing.";
        return result;
    }
    try {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (!reader || !validReader(*reader)) {
            result.error = "Cannot analyze this audio format.";
            return result;
        }

        broke::BeatAnalysisAccumulator accumulator(reader->sampleRate, options.beat);
        if (!accumulator.configured()) {
            result.error = "Invalid beat-analysis configuration.";
            return result;
        }
        const int blockFrames = std::clamp(options.readBlockFrames, 1024, 262144);
        juce::AudioBuffer<float> scratch(2, blockFrames);
        const auto extraForCap = static_cast<juce::int64>(blockFrames);
        const auto capFrames = static_cast<juce::int64>(std::ceil(
            options.beat.maxAnalysisSeconds * reader->sampleRate)) + extraForCap;
        const auto framesToRead = std::min(reader->lengthInSamples, capFrames);

        for (juce::int64 offset = 0; offset < framesToRead;) {
            if (cancelled.load()) {
                result.error = "Analysis cancelled.";
                return result;
            }
            const int count = static_cast<int>(std::min<juce::int64>(
                blockFrames, framesToRead - offset));
            scratch.clear();
            float* destinations[] {scratch.getWritePointer(0), scratch.getWritePointer(1)};
            if (!reader->read(destinations, 2, offset, count)) {
                result.error = "Read error during beat analysis.";
                return result;
            }
            if (reader->numChannels == 1)
                std::copy_n(scratch.getReadPointer(0), count, scratch.getWritePointer(1));
            accumulator.push(scratch.getReadPointer(0), scratch.getReadPointer(1),
                             static_cast<std::size_t>(count));
            offset += count;
        }

        result.beat = accumulator.finish();
        if (!result.beat.valid)
            result.error = "No stable tempo estimate was found.";
    } catch (const std::exception& error) {
        result.error = "Beat analysis failed: " + juce::String(error.what());
    } catch (...) {
        result.error = "Beat analysis failed.";
    }
    return result;
}

TrackRhythmAnalysis analyzeTrackRhythmCached(const juce::File& file,
                                             const std::atomic<bool>& cancelled,
                                             const TrackAnalysisCache& cache,
                                             const TrackAnalysisOptions& options) {
    TrackRhythmAnalysis result;
    if (cancelled.load()) {
        result.error = "Analysis cancelled.";
        return result;
    }
    if (cache.load(file, result.beat)) {
        result.cacheHit = true;
        return result;
    }
    result = analyzeTrackRhythm(file, cancelled, options);
    if (!cancelled.load() && result.beat.valid)
        static_cast<void>(cache.store(file, result.beat));
    return result;
}
