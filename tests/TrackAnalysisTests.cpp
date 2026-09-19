// SPDX-License-Identifier: AGPL-3.0-only
#include "app/TrackAnalysis.h"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
int checks = 0;
void check(bool condition, const char* name) {
    ++checks;
    if (!condition) throw std::runtime_error(name);
}

constexpr double sampleRate = 44100.0;
constexpr double bpm = 120.0;
constexpr double durationSeconds = 20.0;
constexpr int frames = static_cast<int>(sampleRate * durationSeconds);

struct TempFolder final {
    TempFolder()
        : folder(juce::File::getSpecialLocation(juce::File::tempDirectory)
                     .getNonexistentChildFile("BrokeDJ-analysis-test", {}, true)) {
        check(folder.createDirectory(), "temporary analysis folder created");
    }
    ~TempFolder() { folder.deleteRecursively(); }
    juce::File folder;
};

void writeClickTrack(const juce::File& file) {
    juce::AudioBuffer<float> audio(2, frames);
    audio.clear();
    const int beatFrames = static_cast<int>(std::llround(sampleRate * 60.0 / bpm));
    constexpr int clickFrames = 220;
    for (int start = static_cast<int>(0.25 * sampleRate); start < frames; start += beatFrames) {
        for (int i = 0; i < clickFrames && start + i < frames; ++i) {
            const float envelope = 1.0f - static_cast<float>(i) / clickFrames;
            const float value = 0.8f * envelope;
            audio.setSample(0, start + i, value);
            audio.setSample(1, start + i, value * 0.9f);
        }
    }

    juce::WavAudioFormat wav;
    auto stream = file.createOutputStream();
    check(stream != nullptr, "analysis fixture output created");
    auto* raw = stream.release();
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(raw, sampleRate, 2, 16, {}, 0));
    if (!writer) {
        delete raw;
        throw std::runtime_error("analysis fixture writer created");
    }
    check(writer->writeFromAudioSampleBuffer(audio, 0, frames), "analysis fixture written");
    writer.reset();
    check(file.existsAsFile() && file.getSize() > 0, "analysis fixture exists");
}

void runAnalysisAndCache() {
    TempFolder temp;
    const auto source = temp.folder.getChildFile("private-track-name.wav");
    const auto cacheRoot = temp.folder.getChildFile("cache");
    writeClickTrack(source);

    std::atomic<bool> cancelled{false};
    TrackAnalysisOptions options;
    options.beat.maxAnalysisSeconds = 60.0;
    TrackAnalysisCache cache(cacheRoot);

    const auto first = analyzeTrackRhythmCached(source, cancelled, cache, options);
    check(first.error.isEmpty(), "first analysis succeeds");
    check(first.beat.valid, "first analysis returns beat grid");
    check(std::abs(first.beat.bpm - bpm) < 0.8, "file analysis BPM is accurate");
    check(first.beat.confidence > 0.18, "file analysis confidence passes gate");
    check(!first.cacheHit, "first analysis is not a cache hit");
    check(first.beat.segments.size() == 1, "constant-tempo file has one grid segment");

    const auto second = analyzeTrackRhythmCached(source, cancelled, cache, options);
    check(second.error.isEmpty() && second.beat.valid, "cached analysis remains valid");
    check(second.cacheHit, "second analysis uses persistent cache");
    check(std::abs(second.beat.bpm - first.beat.bpm) < 1.0e-9,
          "cached BPM round-trips exactly enough");

    juce::Array<juce::File> cacheFiles;
    cacheRoot.findChildFiles(cacheFiles, juce::File::findFiles, false, "*.analysis");
    check(cacheFiles.size() == 1, "one analysis cache record is written");
    const auto cacheText = cacheFiles[0].loadFileAsString();
    check(!cacheText.containsIgnoreCase(source.getFileName()), "cache does not expose source filename");
    check(!cacheText.containsIgnoreCase(source.getParentDirectory().getFullPathName()),
          "cache does not expose source path");
    check(cacheText.contains("segmentCount=1"), "cache persists beat-grid segment metadata");

    auto variableTempo = first.beat;
    variableTempo.segments.push_back({first.beat.beatZeroSeconds + 4.0, 100.0});
    check(broke::BeatGrid(variableTempo).valid(), "variable-tempo cache fixture is a valid grid");
    check(cache.store(source, variableTempo), "valid variable-tempo map is cacheable");
    broke::BeatAnalysisResult loadedVariable;
    check(cache.load(source, loadedVariable), "variable-tempo cache record reloads");
    check(loadedVariable.segments.size() == 2
          && std::abs(loadedVariable.segments[1].bpm - 100.0) < 1.0e-9,
          "cache round-trips variable-tempo segments");

    auto malformed = variableTempo;
    malformed.segments[1].startSeconds = malformed.segments[0].startSeconds;
    check(!cache.store(source, malformed), "malformed tempo-map ordering is rejected before cache write");
    broke::BeatAnalysisResult stillValid;
    check(cache.load(source, stillValid) && stillValid.segments.size() == 2,
          "rejected cache write preserves the prior valid record");

    const auto changedTime = source.getLastModificationTime() + juce::RelativeTime::seconds(5.0);
    check(source.setLastModificationTime(changedTime), "fixture modification time changes");
    const auto third = analyzeTrackRhythmCached(source, cancelled, cache, options);
    check(third.error.isEmpty() && third.beat.valid, "analysis after source change succeeds");
    check(!third.cacheHit, "source identity change invalidates stale cache");
    cacheFiles.clear();
    cacheRoot.findChildFiles(cacheFiles, juce::File::findFiles, false, "*.analysis");
    check(cacheFiles.size() == 1, "source changes replace rather than multiply cache records");
}

void runFailurePaths() {
    TempFolder temp;
    const auto missing = temp.folder.getChildFile("missing.wav");
    std::atomic<bool> cancelled{false};
    const auto absent = analyzeTrackRhythm(missing, cancelled);
    check(!absent.beat.valid && absent.error.isNotEmpty(), "missing source fails with diagnostic");

    const auto source = temp.folder.getChildFile("cancel.wav");
    writeClickTrack(source);
    cancelled = true;
    const auto stopped = analyzeTrackRhythm(source, cancelled);
    check(!stopped.beat.valid && stopped.error.containsIgnoreCase("cancel"),
          "cancelled analysis exits before decoding");

    cancelled = false;
    TrackAnalysisOptions invalid;
    invalid.beat.minBpm = 180.0;
    invalid.beat.maxBpm = 90.0;
    const auto badOptions = analyzeTrackRhythm(source, cancelled, invalid);
    check(!badOptions.beat.valid && badOptions.error.isNotEmpty(),
          "invalid analysis options fail closed");
}
}

int main() {
    try {
        runAnalysisAndCache();
        runFailurePaths();
        std::cout << "PASS: " << checks << " track-analysis/cache checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
