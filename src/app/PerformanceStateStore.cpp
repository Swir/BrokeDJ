// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#include "PerformanceStateStore.h"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <utility>

namespace {
constexpr int hotCueSchema = 1;
constexpr std::size_t maxStateBytes = 16 * 1024;
constexpr double maxTrackSeconds = 24.0 * 60.0 * 60.0;
constexpr double maxBeatStep = 64.0;

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

bool parseBool(const juce::String& value, bool& result) {
    if (value == "0") {
        result = false;
        return true;
    }
    if (value == "1") {
        result = true;
        return true;
    }
    return false;
}

bool validCue(const TrackHotCueSnapshot::Cue& cue) {
    if (!cue.set) return true;
    return std::isfinite(cue.seconds)
        && cue.seconds >= 0.0 && cue.seconds <= maxTrackSeconds
        && std::isfinite(cue.beatStep)
        && cue.beatStep > 0.0 && cue.beatStep <= maxBeatStep;
}
} // namespace

TrackHotCueStore::TrackHotCueStore(juce::File rootIn) : root(std::move(rootIn)) {}

juce::File TrackHotCueStore::defaultRoot() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("BrokeDJ")
        .getChildFile("hotcues-v1");
}

juce::File TrackHotCueStore::stateFileFor(const juce::File& source) const {
    // The local path is used only as input to the on-disk key. The payload itself
    // contains only file identity fields and derived cue metadata.
    return root.getChildFile(hex64(fnv1a64(source.getFullPathName())) + ".hotcues");
}

bool TrackHotCueStore::load(const juce::File& source, TrackHotCueSnapshot& snapshot) const {
    snapshot = {};
    if (!source.existsAsFile()) return false;

    const auto stored = stateFileFor(source);
    const auto storedSize = stored.getSize();
    if (!stored.existsAsFile() || storedSize <= 0
        || static_cast<std::uint64_t>(storedSize) > static_cast<std::uint64_t>(maxStateBytes)) {
        return false;
    }

    const auto fields = parseFields(stored.loadFileAsString());
    const juce::String empty;
    if (fields.getValue("schema", empty).getIntValue() != hotCueSchema
        || fields.getValue("fileSize", empty).getLargeIntValue() != source.getSize()
        || fields.getValue("modifiedMs", empty).getLargeIntValue()
            != source.getLastModificationTime().toMilliseconds()
        || fields.getValue("cueCount", empty).getIntValue()
            != static_cast<int>(TrackHotCueSnapshot::cueCount)) {
        return false;
    }

    TrackHotCueSnapshot decoded;
    for (std::size_t i = 0; i < decoded.cues.size(); ++i) {
        const auto prefix = "cue" + juce::String(static_cast<int>(i));
        bool isSet = false;
        if (!parseBool(fields.getValue(prefix + "Set", empty), isSet)) return false;
        if (!isSet) continue;

        bool quantized = false;
        if (!parseBool(fields.getValue(prefix + "Quantized", empty), quantized)) return false;

        auto& cue = decoded.cues[i];
        cue.set = true;
        cue.seconds = fields.getValue(prefix + "Seconds", empty).getDoubleValue();
        cue.quantized = quantized;
        cue.beatStep = fields.getValue(prefix + "BeatStep", empty).getDoubleValue();
        if (!validCue(cue)) return false;
    }

    snapshot = decoded;
    return true;
}

bool TrackHotCueStore::store(const juce::File& source,
                             const TrackHotCueSnapshot& snapshot) const {
    if (!source.existsAsFile()) return false;
    for (const auto& cue : snapshot.cues)
        if (!validCue(cue)) return false;
    if (!root.isDirectory() && !root.createDirectory()) return false;

    juce::String payload;
    payload << "schema=" << hotCueSchema << "\n";
    payload << "fileSize=" << source.getSize() << "\n";
    payload << "modifiedMs=" << source.getLastModificationTime().toMilliseconds() << "\n";
    payload << "cueCount=" << static_cast<int>(snapshot.cues.size()) << "\n";
    for (std::size_t i = 0; i < snapshot.cues.size(); ++i) {
        const auto& cue = snapshot.cues[i];
        const auto prefix = "cue" + juce::String(static_cast<int>(i));
        payload << prefix << "Set=" << (cue.set ? 1 : 0) << "\n";
        if (!cue.set) continue;
        payload << prefix << "Seconds=" << juce::String(cue.seconds, 12) << "\n";
        payload << prefix << "Quantized=" << (cue.quantized ? 1 : 0) << "\n";
        payload << prefix << "BeatStep=" << juce::String(cue.beatStep, 12) << "\n";
    }

    if (payload.getNumBytesAsUTF8() > maxStateBytes) return false;

    const auto target = stateFileFor(source);
    juce::TemporaryFile temporary(target);
    if (!temporary.getFile().replaceWithText(payload, false, false, "\n")) return false;
    return temporary.overwriteTargetFileWithTemporary();
}

bool TrackHotCueStore::erase(const juce::File& source) const {
    if (source.getFullPathName().isEmpty()) return false;
    const auto stored = stateFileFor(source);
    return !stored.existsAsFile() || stored.deleteFile();
}
