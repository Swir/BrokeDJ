// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#include "BeatGridStore.h"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <utility>

namespace {
constexpr int gridSchema = 1;
constexpr std::int64_t maxGridBytes = 64 * 1024;
constexpr int maxStoredSegments = static_cast<int>(broke::BeatGrid::maxSegments);

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

bool finiteRange(double value, double minimum, double maximum) {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
}

BeatGridStore::BeatGridStore(juce::File rootIn) : root(std::move(rootIn)) {}

juce::File BeatGridStore::defaultRoot() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("BrokeDJ")
        .getChildFile("beat-grid-edits-v1");
}

juce::File BeatGridStore::gridFileFor(const juce::File& source) const {
    // The local path is used only to derive an opaque filename. It is never
    // copied into the persisted payload or diagnostics.
    return root.getChildFile(hex64(fnv1a64(source.getFullPathName())) + ".grid");
}

bool BeatGridStore::load(const juce::File& source, broke::BeatGrid& grid) const {
    grid = {};
    if (!source.existsAsFile()) return false;

    const auto stored = gridFileFor(source);
    if (!stored.existsAsFile() || stored.getSize() <= 0 || stored.getSize() > maxGridBytes)
        return false;

    const auto fields = parseFields(stored.loadFileAsString());
    const juce::String empty;
    if (fields.getValue("schema", empty).getIntValue() != gridSchema
        || fields.getValue("fileSize", empty).getLargeIntValue() != source.getSize()
        || fields.getValue("modifiedMs", empty).getLargeIntValue()
            != source.getLastModificationTime().toMilliseconds()) {
        return false;
    }

    const double beatZero = fields.getValue("beatZeroSeconds", empty).getDoubleValue();
    const int segmentCount = fields.getValue("segmentCount", empty).getIntValue();
    if (!finiteRange(beatZero, 0.0, 24.0 * 60.0 * 60.0)
        || segmentCount <= 0 || segmentCount > maxStoredSegments) {
        return false;
    }

    broke::BeatAnalysisResult snapshot;
    snapshot.valid = true;
    snapshot.beatZeroSeconds = beatZero;
    snapshot.segments.reserve(static_cast<std::size_t>(segmentCount));
    for (int i = 0; i < segmentCount; ++i) {
        const auto suffix = juce::String(i);
        const double start = fields.getValue("segment" + suffix + "Start", empty).getDoubleValue();
        const double bpm = fields.getValue("segment" + suffix + "Bpm", empty).getDoubleValue();
        if (!finiteRange(start, 0.0, 24.0 * 60.0 * 60.0)
            || !finiteRange(bpm, 30.0, 300.0)) {
            return false;
        }
        snapshot.segments.push_back({start, bpm});
    }
    snapshot.bpm = snapshot.segments.front().bpm;

    broke::BeatGrid decoded(snapshot);
    if (!decoded.valid()) return false;
    grid = std::move(decoded);
    return true;
}

bool BeatGridStore::store(const juce::File& source, const broke::BeatGrid& grid) const {
    if (!source.existsAsFile() || !grid.valid()) return false;
    const auto& segments = grid.segments();
    if (segments.empty() || segments.size() > broke::BeatGrid::maxSegments) return false;
    if (!root.isDirectory() && !root.createDirectory()) return false;

    juce::String payload;
    payload << "schema=" << gridSchema << "\n";
    payload << "fileSize=" << source.getSize() << "\n";
    payload << "modifiedMs=" << source.getLastModificationTime().toMilliseconds() << "\n";
    payload << "beatZeroSeconds=" << juce::String(grid.beatZeroSeconds(), 12) << "\n";
    payload << "segmentCount=" << static_cast<int>(segments.size()) << "\n";
    for (std::size_t i = 0; i < segments.size(); ++i) {
        payload << "segment" << static_cast<int>(i) << "Start="
                << juce::String(segments[i].startSeconds, 12) << "\n";
        payload << "segment" << static_cast<int>(i) << "Bpm="
                << juce::String(segments[i].bpm, 12) << "\n";
    }

    const auto target = gridFileFor(source);
    juce::TemporaryFile temporary(target);
    if (!temporary.getFile().replaceWithText(payload, false, false, "\n")) return false;
    return temporary.overwriteTargetFileWithTemporary();
}

bool BeatGridStore::erase(const juce::File& source) const {
    if (source.getFullPathName().isEmpty()) return false;
    const auto stored = gridFileFor(source);
    return !stored.existsAsFile() || stored.deleteFile();
}
