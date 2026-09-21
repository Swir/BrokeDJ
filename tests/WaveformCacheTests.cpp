// SPDX-License-Identifier: AGPL-3.0-only
#include "app/WaveformCache.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
int checks = 0;

void check(bool condition, const char* name) {
    ++checks;
    if (!condition) throw std::runtime_error(name);
}

struct TempRoot final {
    juce::File directory;

    TempRoot() {
        directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile("BrokeDJ-waveform-cache-test", "", false);
        check(directory.createDirectory(), "temporary waveform-cache root created");
    }

    ~TempRoot() { static_cast<void>(directory.deleteRecursively()); }
};

juce::File makeSource(const juce::File& root, const juce::String& name,
                      const juce::String& body = "synthetic-waveform-source") {
    const auto source = root.getChildFile(name);
    check(source.replaceWithText(body, false, false, "\n"), "source fixture created");
    return source;
}

std::vector<float> makePeaks(float scale = 1.0f) {
    std::vector<float> peaks(WaveformPreviewCache::peakCount);
    for (std::size_t i = 0; i < peaks.size(); ++i) {
        const auto phase = static_cast<float>(i % 97u) / 96.0f;
        peaks[i] = std::min(1.0f, phase * scale);
    }
    return peaks;
}

juce::File onlyCacheFile(const juce::File& root) {
    const auto files = root.findChildFiles(juce::File::findFiles, false, "*.waveform");
    check(files.size() == 1, "exactly one waveform cache file exists");
    return files.getFirst();
}

void roundTripIsBoundedPrivateAndSourceBound() {
    TempRoot temp;
    const auto source = makeSource(temp.directory, "private-track-name.wav");
    const auto cacheRoot = temp.directory.getChildFile("cache");
    WaveformPreviewCache cache(cacheRoot);
    const auto expected = makePeaks();

    check(cache.store(source, expected), "valid waveform preview stores");
    std::vector<float> loaded;
    check(cache.load(source, loaded), "waveform preview reloads");
    check(loaded.size() == WaveformPreviewCache::peakCount,
          "waveform preview keeps bounded point count");
    for (std::size_t i = 0; i < loaded.size(); ++i) {
        check(std::abs(loaded[i] - expected[i]) < 1.0e-6f,
              "waveform preview values round-trip");
    }

    const auto cacheFile = onlyCacheFile(cacheRoot);
    const auto payload = cacheFile.loadFileAsString();
    check(!payload.contains(source.getFullPathName()),
          "waveform payload omits raw source path");
    check(!payload.contains(source.getFileName()),
          "waveform payload omits source filename");
    check(!cacheFile.getFileName().containsIgnoreCase(source.getFileNameWithoutExtension()),
          "waveform cache filename does not expose source name");

    const auto changedTime = source.getLastModificationTime() + juce::RelativeTime::seconds(5.0);
    check(source.setLastModificationTime(changedTime),
          "source modification time can change for invalidation fixture");
    loaded.assign(4, 0.5f);
    check(!cache.load(source, loaded),
          "source identity change rejects stale waveform cache");
    check(loaded.empty(), "failed cache load clears destination peaks");
}

void rewritesAreAtomicAndInvalidInputDoesNotClobber() {
    TempRoot temp;
    const auto source = makeSource(temp.directory, "rewrite.flac");
    const auto cacheRoot = temp.directory.getChildFile("cache");
    WaveformPreviewCache cache(cacheRoot);

    const auto first = makePeaks(0.45f);
    const auto second = makePeaks(0.90f);
    check(cache.store(source, first), "initial waveform preview stores");
    check(cache.store(source, second), "replacement waveform preview stores atomically");

    std::vector<float> loaded;
    check(cache.load(source, loaded), "replacement waveform preview reloads");
    check(loaded.size() == second.size() && std::abs(loaded[96] - second[96]) < 1.0e-6f,
          "latest complete waveform snapshot wins");

    auto invalid = second;
    invalid[7] = std::numeric_limits<float>::quiet_NaN();
    check(!cache.store(source, invalid), "non-finite waveform point is rejected");
    check(cache.load(source, loaded) && std::abs(loaded[96] - second[96]) < 1.0e-6f,
          "rejected non-finite write preserves prior snapshot");

    invalid = second;
    invalid[8] = 1.01f;
    check(!cache.store(source, invalid), "out-of-range waveform point is rejected");
    check(cache.load(source, loaded) && std::abs(loaded[96] - second[96]) < 1.0e-6f,
          "rejected out-of-range write preserves prior snapshot");

    auto wrongCount = second;
    wrongCount.pop_back();
    check(!cache.store(source, wrongCount), "wrong waveform point count is rejected");
    check(cache.load(source, loaded) && loaded.size() == WaveformPreviewCache::peakCount,
          "wrong-count write does not clobber prior snapshot");
}

void malformedCacheFailsClosed() {
    TempRoot temp;
    const auto source = makeSource(temp.directory, "corrupt.ogg");
    const auto cacheRoot = temp.directory.getChildFile("cache");
    WaveformPreviewCache cache(cacheRoot);
    const auto expected = makePeaks();
    check(cache.store(source, expected), "corruption fixture stores");
    const auto cacheFile = onlyCacheFile(cacheRoot);

    check(cacheFile.replaceWithText("schema=999\nfileSize=1\nmodifiedMs=1\ncount=512\n",
                                    false, false, "\n"),
          "future-schema cache fixture written");
    std::vector<float> loaded{0.5f};
    check(!cache.load(source, loaded), "unknown cache schema fails closed");
    check(loaded.empty(), "unknown schema leaves no stale destination peaks");

    check(cache.store(source, expected), "valid cache restored after schema fixture");
    auto payload = cacheFile.loadFileAsString();
    payload = payload.replace("count=512", "count=511");
    check(cacheFile.replaceWithText(payload, false, false, "\n"),
          "wrong-count cache fixture written");
    check(!cache.load(source, loaded), "wrong peak count fails closed");

    check(cache.store(source, expected), "valid cache restored after count fixture");
    payload = cacheFile.loadFileAsString();
    payload = payload.replace("p10=", "p10=nan\ninvalid=");
    check(cacheFile.replaceWithText(payload, false, false, "\n"),
          "non-finite cache fixture written");
    check(!cache.load(source, loaded), "non-finite cached point fails closed");

    check(cache.store(source, expected), "valid cache restored after non-finite fixture");
    check(source.deleteFile(), "source can be removed for missing-file fixture");
    check(!cache.load(source, loaded), "missing source never returns cached waveform");
}
} // namespace

int main() {
    try {
        roundTripIsBoundedPrivateAndSourceBound();
        rewritesAreAtomicAndInvalidInputDoesNotClobber();
        malformedCacheFailsClosed();
        std::cout << "WaveformCacheTests: " << checks << " checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "WaveformCacheTests failed after " << checks
                  << " checks: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
