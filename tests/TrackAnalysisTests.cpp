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
constexpr double pi = 3.141592653589793238462643383279502884;

double midiFrequency(int midi) {
    return 440.0 * std::pow(2.0, (static_cast<double>(midi) - 69.0) / 12.0);
}

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

    // Add a deterministic C-major harmonic bed underneath the transient click
    // track so one decode pass exercises both tempo and key analysis. The chord
    // progression is C -> F -> G -> C and intentionally remains synthetic test
    // evidence rather than a claim about arbitrary real-world music accuracy.
    constexpr int chords[4][3] {{60, 64, 67}, {65, 69, 72}, {67, 71, 74}, {60, 64, 67}};
    const int framesPerChord = frames / 4;
    for (int frame = 0; frame < frames; ++frame) {
        const int chord = std::min(3, frame / framesPerChord);
        const double time = static_cast<double>(frame) / sampleRate;
        double tonal = 0.0;
        for (int note = 0; note < 3; ++note)
            tonal += std::sin(2.0 * pi * midiFrequency(chords[chord][note]) * time);
        const float value = static_cast<float>(0.12 * tonal / 3.0);
        audio.setSample(0, frame, value);
        audio.setSample(1, frame, value * 0.96f);
    }

    const int beatFrames = static_cast<int>(std::llround(sampleRate * 60.0 / bpm));
    constexpr int clickFrames = 220;
    for (int start = static_cast<int>(0.25 * sampleRate); start < frames; start += beatFrames) {
        for (int i = 0; i < clickFrames && start + i < frames; ++i) {
            const float envelope = 1.0f - static_cast<float>(i) / clickFrames;
            const float value = 0.75f * envelope;
            audio.addSample(0, start + i, value);
            audio.addSample(1, start + i, value * 0.9f);
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
    options.key.maxAnalysisSeconds = 60.0;
    TrackAnalysisCache cache(cacheRoot);

    const auto first = analyzeTrackRhythmCached(source, cancelled, cache, options);
    check(first.error.isEmpty(), "first combined analysis succeeds");
    check(first.beat.valid, "first analysis returns beat grid");
    check(std::abs(first.beat.bpm - bpm) < 0.8, "file analysis BPM is accurate");
    check(first.beat.confidence > 0.18, "file analysis tempo confidence passes gate");
    check(first.beat.segments.size() == 1, "constant-tempo file has one grid segment");
    check(first.key.valid, "same decode pass returns musical key");
    check(first.key.tonic == 0, "C-major file fixture reports C tonic");
    check(first.key.mode == broke::KeyMode::major, "C-major file fixture reports major mode");
    check(first.key.confidence > 0.20, "file key confidence passes deterministic gate");
    check(!first.cacheHit, "first analysis is not a cache hit");

    const auto second = analyzeTrackRhythmCached(source, cancelled, cache, options);
    check(second.error.isEmpty(), "cached combined analysis remains valid");
    check(second.cacheHit, "second analysis uses persistent cache");
    check(second.beat.valid && second.key.valid, "cache restores beat and key metadata");
    check(std::abs(second.beat.bpm - first.beat.bpm) < 1.0e-9,
          "cached BPM round-trips exactly enough");
    check(second.key.tonic == first.key.tonic && second.key.mode == first.key.mode,
          "cached key identity round-trips");

    juce::Array<juce::File> cacheFiles;
    cacheRoot.findChildFiles(cacheFiles, juce::File::findFiles, false, "*.analysis");
    check(cacheFiles.size() == 1, "one analysis cache record is written");
    const auto cacheText = cacheFiles[0].loadFileAsString();
    check(!cacheText.containsIgnoreCase(source.getFileName()), "cache does not expose source filename");
    check(!cacheText.containsIgnoreCase(source.getParentDirectory().getFullPathName()),
          "cache does not expose source path");
    check(cacheText.contains("segmentCount=1"), "cache persists beat-grid segment metadata");
    check(cacheText.contains("keyValid=1"), "cache persists validated musical-key metadata");

    auto variableTempo = first.beat;
    variableTempo.segments.push_back({first.beat.beatZeroSeconds + 4.0, 100.0});
    check(broke::BeatGrid(variableTempo).valid(), "variable-tempo cache fixture is a valid grid");
    check(cache.store(source, variableTempo, first.key), "valid variable-tempo/key metadata is cacheable");
    broke::BeatAnalysisResult loadedVariable;
    broke::MusicalKeyResult loadedKey;
    check(cache.load(source, loadedVariable, loadedKey), "variable-tempo/key cache record reloads");
    check(loadedVariable.segments.size() == 2
          && std::abs(loadedVariable.segments[1].bpm - 100.0) < 1.0e-9,
          "cache round-trips variable-tempo segments");
    check(loadedKey.valid && loadedKey.tonic == 0 && loadedKey.mode == broke::KeyMode::major,
          "cache round-trips musical key with edited grid");

    auto malformed = variableTempo;
    malformed.segments[1].startSeconds = malformed.segments[0].startSeconds;
    check(!cache.store(source, malformed, first.key),
          "malformed tempo-map ordering is rejected before cache write");
    broke::BeatAnalysisResult stillValid;
    broke::MusicalKeyResult stillValidKey;
    check(cache.load(source, stillValid, stillValidKey)
          && stillValid.segments.size() == 2 && stillValidKey.valid,
          "rejected cache write preserves the prior valid metadata record");

    auto malformedKey = first.key;
    malformedKey.tonic = 12;
    check(!cache.store(source, variableTempo, malformedKey),
          "out-of-range musical key is rejected before cache write");

    const auto changedTime = source.getLastModificationTime() + juce::RelativeTime::seconds(5.0);
    check(source.setLastModificationTime(changedTime), "fixture modification time changes");
    const auto third = analyzeTrackRhythmCached(source, cancelled, cache, options);
    check(third.error.isEmpty() && third.beat.valid && third.key.valid,
          "combined analysis after source change succeeds");
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
    check(!absent.beat.valid && !absent.key.valid && absent.error.isNotEmpty(),
          "missing source fails both analyzers with diagnostic");

    const auto source = temp.folder.getChildFile("cancel.wav");
    writeClickTrack(source);
    cancelled = true;
    const auto stopped = analyzeTrackRhythm(source, cancelled);
    check(!stopped.beat.valid && !stopped.key.valid && stopped.error.containsIgnoreCase("cancel"),
          "cancelled analysis exits before decoding");

    cancelled = false;
    TrackAnalysisOptions invalidBeat;
    invalidBeat.beat.minBpm = 180.0;
    invalidBeat.beat.maxBpm = 90.0;
    const auto badBeatOptions = analyzeTrackRhythm(source, cancelled, invalidBeat);
    check(!badBeatOptions.beat.valid && !badBeatOptions.key.valid && badBeatOptions.error.isNotEmpty(),
          "invalid beat-analysis configuration fails closed before worker decode");

    TrackAnalysisOptions invalidKey;
    invalidKey.key.targetRate = 1000.0;
    const auto badKeyOptions = analyzeTrackRhythm(source, cancelled, invalidKey);
    check(!badKeyOptions.beat.valid && !badKeyOptions.key.valid && badKeyOptions.error.isNotEmpty(),
          "invalid key-analysis configuration fails closed before worker decode");
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
