// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <juce_core/juce_core.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

// Persistent, privacy-conscious cache for the 512-point amplitude preview used
// by the native decks. The source pathname is used only to derive the cache-file
// key; it is never written into the payload. Size + modification time are stored
// as validation fields so a replaced/edited source fails closed and is rebuilt.
// All methods perform file I/O and therefore belong to decoder/background workers,
// never the real-time audio callback.
class WaveformPreviewCache final {
public:
    static constexpr std::size_t peakCount = 512;

    explicit WaveformPreviewCache(juce::File rootIn = defaultRoot())
        : root(std::move(rootIn)) {}

    [[nodiscard]] bool load(const juce::File& source, std::vector<float>& peaks) const {
        peaks.clear();
        if (!source.existsAsFile()) return false;

        const auto cached = cacheFileFor(source);
        if (!cached.existsAsFile() || cached.getSize() <= 0 || cached.getSize() > maxCacheBytes)
            return false;

        const auto fields = parseFields(cached.loadFileAsString());
        const juce::String empty;
        if (fields.getValue("schema", empty).getIntValue() != schemaVersion
            || fields.getValue("fileSize", empty).getLargeIntValue() != source.getSize()
            || fields.getValue("modifiedMs", empty).getLargeIntValue()
                != source.getLastModificationTime().toMilliseconds()
            || fields.getValue("count", empty).getIntValue()
                != static_cast<int>(peakCount)) {
            return false;
        }

        std::vector<float> decoded;
        decoded.reserve(peakCount);
        for (std::size_t i = 0; i < peakCount; ++i) {
            const auto text = fields.getValue("p" + juce::String(static_cast<int>(i)), empty);
            if (text.isEmpty()) return false;
            const double value = text.getDoubleValue();
            if (!std::isfinite(value) || value < 0.0 || value > 1.0) return false;
            decoded.push_back(static_cast<float>(value));
        }
        peaks = std::move(decoded);
        return true;
    }

    [[nodiscard]] bool store(const juce::File& source,
                             const std::vector<float>& peaks) const {
        if (!source.existsAsFile() || peaks.size() != peakCount) return false;
        for (const float value : peaks) {
            if (!std::isfinite(value) || value < 0.0f || value > 1.0f) return false;
        }

        if (!root.isDirectory() && !root.createDirectory()) return false;

        juce::String payload;
        payload.preallocateBytes(peakCount * 20 + 256);
        payload << "schema=" << schemaVersion << "\n";
        payload << "fileSize=" << source.getSize() << "\n";
        payload << "modifiedMs=" << source.getLastModificationTime().toMilliseconds() << "\n";
        payload << "count=" << static_cast<int>(peakCount) << "\n";
        for (std::size_t i = 0; i < peaks.size(); ++i) {
            payload << "p" << static_cast<int>(i) << "="
                    << juce::String(static_cast<double>(peaks[i]), 9) << "\n";
        }
        if (payload.getNumBytesAsUTF8() <= 0 || payload.getNumBytesAsUTF8() > maxCacheBytes)
            return false;

        const auto target = cacheFileFor(source);
        juce::TemporaryFile temporary(target);
        if (!temporary.getFile().replaceWithText(payload, false, false, "\n")) return false;
        return temporary.overwriteTargetFileWithTemporary();
    }

    [[nodiscard]] const juce::File& rootDirectory() const noexcept { return root; }

    [[nodiscard]] static juce::File defaultRoot() {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("BrokeDJ")
            .getChildFile("waveform-v1");
    }

private:
    static constexpr int schemaVersion = 1;
    static constexpr std::int64_t maxCacheBytes = 64 * 1024;

    [[nodiscard]] static std::uint64_t fnv1a64(const juce::String& text) {
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

    [[nodiscard]] static juce::String hex64(std::uint64_t value) {
        std::ostringstream stream;
        stream << std::hex << std::setw(16) << std::setfill('0') << value;
        return juce::String(stream.str());
    }

    [[nodiscard]] static juce::StringPairArray parseFields(const juce::String& text) {
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

    [[nodiscard]] juce::File cacheFileFor(const juce::File& source) const {
        return root.getChildFile(hex64(fnv1a64(source.getFullPathName())) + ".waveform");
    }

    juce::File root;
};
