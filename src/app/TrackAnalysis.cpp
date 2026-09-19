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
constexpr int cacheSchema = 2;
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

bool validKeyResult(const broke::MusicalKeyResult& result) {
    return result.valid
        && result.tonic >= 0 && result.tonic < 12
        && (result.mode == broke::KeyMode::major || result.mode == broke::KeyMode::minor)
        && finiteRange(result.confidence, 0.0, 1.0)
        && finiteRange(result.analyzedSeconds, 0.0, 4.0 * 60.0 * 60.0);
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
        .getChildFile("analysis-v2");
}

juce::File TrackAnalysisCache::cacheFileFor(const juce::File& source) const {
    // Hash the local path only for the on-disk key. Size and mtime are stored as
    // validation fields so a changed file replaces its prior cache record rather
    // than accumulating one cache file per edit. The path itself is never stored.
    return root.getChildFile(hex64(fnv1a64(source.getFullPathName())) + ".analysis");
}

bool TrackAnalysisCache::load(const juce::File& source,
                              broke::BeatAnalysisResult& beat,
                              broke::MusicalKeyResult& key) const {
    beat = {};
    key = {};
    if (!source.existsAsFile()) return false;
    const auto cached = cacheFileFor(source);
    if (!cached.existsAsFile() || cached.getSize() <= 0 || cached.getSize() > maxCacheBytes)
        return false;

    const auto fields = parseFields(cached.loadFileAsString());
    const juce::String empty;
    if (fields.getValue("schema", empty).getIntValue() != cacheSchema
        || fields.getValue("fileSize", empty).getLargeIntValue() != source.getSize()
        || fields.getValue("modifiedMs", empty).getLargeIntValue()
            != source.getLastModificationTime().toMilliseconds()) {
        return false;
    }

    const bool beatValid = fields.getValue("beatValid", empty) == "1";
    const bool keyValid = fields.getValue("keyValid", empty) == "1";
    if (!beatValid && !keyValid) return false;

    if (beatValid) {
        const double bpm = fields.getValue("bpm", empty).getDoubleValue();
        const double beatZero = fields.getValue("beatZeroSeconds", empty).getDoubleValue();
        const double confidence = fields.getValue("beatConfidence", empty).getDoubleValue();
        const double analyzedSeconds = fields.getValue("beatAnalyzedSeconds", empty).getDoubleValue();
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
        decoded.truncated = fields.getValue("beatTruncated", empty) == "1";
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
        // Treat local cache state as untrusted. Range checks are not enough:
        // segment zero must equal beat zero and later boundaries must increase.
        if (!broke::BeatGrid(decoded).valid()) return false;
        beat = std::move(decoded);
    }

    if (keyValid) {
        broke::MusicalKeyResult decoded;
        decoded.valid = true;
        decoded.tonic = fields.getValue("keyTonic", empty).getIntValue();
        const int mode = fields.getValue("keyMode", empty).getIntValue();
        decoded.mode = mode == static_cast<int>(broke::KeyMode::major)
            ? broke::KeyMode::major
            : (mode == static_cast<int>(broke::KeyMode::minor)
                ? broke::KeyMode::minor : broke::KeyMode::unknown);
        decoded.confidence = fields.getValue("keyConfidence", empty).getDoubleValue();
        decoded.analyzedSeconds = fields.getValue("keyAnalyzedSeconds", empty).getDoubleValue();
        decoded.truncated = fields.getValue("keyTruncated", empty) == "1";
        if (!validKeyResult(decoded)) return false;
        key = decoded;
    }

    return true;
}

bool TrackAnalysisCache::store(const juce::File& source,
                               const broke::BeatAnalysisResult& beat,
                               const broke::MusicalKeyResult& key) const {
    if (!source.existsAsFile() || (!beat.valid && !key.valid)) return false;

    if (beat.valid) {
        if (!finiteRange(beat.bpm, 30.0, 300.0)
            || !finiteRange(beat.beatZeroSeconds, 0.0, 24.0 * 60.0 * 60.0)
            || !finiteRange(beat.confidence, 0.0, 1.0)
            || !finiteRange(beat.analyzedSeconds, 0.0, 4.0 * 60.0 * 60.0)
            || beat.segments.empty()
            || beat.segments.size() > static_cast<std::size_t>(maxCachedSegments)
            || !broke::BeatGrid(beat).valid()) {
            return false;
        }
        for (const auto& segment : beat.segments) {
            if (!finiteRange(segment.startSeconds, 0.0, 24.0 * 60.0 * 60.0)
                || !finiteRange(segment.bpm, 30.0, 300.0)) return false;
        }
    }
    if (key.valid && !validKeyResult(key)) return false;
    if (!root.isDirectory() && !root.createDirectory()) return false;

    juce::String payload;
    payload << "schema=" << cacheSchema << "\n";
    payload << "fileSize=" << source.getSize() << "\n";
    payload << "modifiedMs=" << source.getLastModificationTime().toMilliseconds() << "\n";
    payload << "beatValid=" << (beat.valid ? 1 : 0) << "\n";
    payload << "keyValid=" << (key.valid ? 1 : 0) << "\n";

    if (beat.valid) {
        payload << "bpm=" << juce::String(beat.bpm, 12) << "\n";
        payload << "beatZeroSeconds=" << juce::String(beat.beatZeroSeconds, 12) << "\n";
        payload << "beatConfidence=" << juce::String(beat.confidence, 12) << "\n";
        payload << "beatAnalyzedSeconds=" << juce::String(beat.analyzedSeconds, 12) << "\n";
        payload << "beatTruncated=" << (beat.truncated ? 1 : 0) << "\n";
        payload << "segmentCount=" << static_cast<int>(beat.segments.size()) << "\n";
        for (std::size_t i = 0; i < beat.segments.size(); ++i) {
            payload << "segment" << static_cast<int>(i) << "Start="
                    << juce::String(beat.segments[i].startSeconds, 12) << "\n";
            payload << "segment" << static_cast<int>(i) << "Bpm="
                    << juce::String(beat.segments[i].bpm, 12) << "\n";
        }
    }

    if (key.valid) {
        payload << "keyTonic=" << key.tonic << "\n";
        payload << "keyMode=" << static_cast<int>(key.mode) << "\n";
        payload << "keyConfidence=" << juce::String(key.confidence, 12) << "\n";
        payload << "keyAnalyzedSeconds=" << juce::String(key.analyzedSeconds, 12) << "\n";
        payload << "keyTruncated=" << (key.truncated ? 1 : 0) << "\n";
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

        broke::BeatAnalysisAccumulator beatAccumulator(reader->sampleRate, options.beat);
        broke::KeyAnalysisAccumulator keyAccumulator(reader->sampleRate, options.key);
        if (!beatAccumulator.configured() || !keyAccumulator.configured()) {
            result.error = "Invalid musical-analysis configuration.";
            return result;
        }

        const int blockFrames = std::clamp(options.readBlockFrames, 1024, 262144);
        juce::AudioBuffer<float> scratch(2, blockFrames);
        const auto extraForCap = static_cast<juce::int64>(blockFrames);
        const double requestedSeconds = std::max(
            options.beat.maxAnalysisSeconds, options.key.maxAnalysisSeconds);
        const auto capFrames = static_cast<juce::int64>(std::ceil(
            requestedSeconds * reader->sampleRate)) + extraForCap;
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
                result.error = "Read error during musical analysis.";
                return result;
            }
            if (reader->numChannels == 1)
                std::copy_n(scratch.getReadPointer(0), count, scratch.getWritePointer(1));

            beatAccumulator.push(scratch.getReadPointer(0), scratch.getReadPointer(1),
                                 static_cast<std::size_t>(count));
            keyAccumulator.push(scratch.getReadPointer(0), scratch.getReadPointer(1),
                                static_cast<std::size_t>(count));
            offset += count;
        }

        result.beat = beatAccumulator.finish();
        result.key = keyAccumulator.finish();
        if (!result.beat.valid)
            result.beatError = "No stable tempo estimate was found.";
        if (!result.key.valid)
            result.keyError = "No stable musical-key estimate was found.";
    } catch (const std::exception& error) {
        result.error = "Musical analysis failed: " + juce::String(error.what());
    } catch (...) {
        result.error = "Musical analysis failed.";
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
    if (cache.load(file, result.beat, result.key)) {
        result.cacheHit = true;
        if (!result.beat.valid)
            result.beatError = "No cached tempo estimate is available.";
        if (!result.key.valid)
            result.keyError = "No cached musical-key estimate is available.";
        return result;
    }

    result = analyzeTrackRhythm(file, cancelled, options);
    if (!cancelled.load() && (result.beat.valid || result.key.valid))
        static_cast<void>(cache.store(file, result.beat, result.key));
    return result;
}
