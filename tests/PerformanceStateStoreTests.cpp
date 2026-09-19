// SPDX-License-Identifier: AGPL-3.0-only
#include "app/PerformanceStateStore.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

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
            .getNonexistentChildFile("BrokeDJ-hotcue-store-test", "", false);
        check(directory.createDirectory(), "temporary hotcue root created");
    }

    ~TempRoot() { static_cast<void>(directory.deleteRecursively()); }
};

juce::File makeSource(const juce::File& root, const juce::String& name,
                      const juce::String& body = "fixture-audio-identity") {
    const auto source = root.getChildFile(name);
    check(source.replaceWithText(body, false, false, "\n"), "source fixture created");
    return source;
}

juce::File onlyStateFile(const juce::File& root) {
    const auto files = root.findChildFiles(juce::File::findFiles, false, "*.hotcues");
    check(files.size() == 1, "exactly one hotcue state file exists");
    return files.getFirst();
}

void roundTripIsSourceBoundAndPrivate() {
    TempRoot temp;
    const auto source = makeSource(temp.directory, "private-track-name.wav");
    const auto stateRoot = temp.directory.getChildFile("state");
    TrackHotCueStore store(stateRoot);

    TrackHotCueSnapshot snapshot;
    snapshot.cues[0] = {true, 1.25, true, 1.0};
    snapshot.cues[3] = {true, 42.125, true, 0.5};
    snapshot.cues[7] = {true, 59.75, false, 4.0};
    check(store.store(source, snapshot), "valid source-bound hotcues persist");

    TrackHotCueSnapshot loaded;
    check(store.load(source, loaded), "persisted hotcues reload");
    check(loaded.cues[0].set && loaded.cues[0].quantized
              && std::abs(loaded.cues[0].seconds - 1.25) < 1.0e-12
              && std::abs(loaded.cues[0].beatStep - 1.0) < 1.0e-12,
          "quantized cue metadata round-trips");
    check(loaded.cues[3].set && std::abs(loaded.cues[3].seconds - 42.125) < 1.0e-12
              && std::abs(loaded.cues[3].beatStep - 0.5) < 1.0e-12,
          "fractional beat-step cue round-trips");
    check(loaded.cues[7].set && !loaded.cues[7].quantized
              && std::abs(loaded.cues[7].seconds - 59.75) < 1.0e-12,
          "unquantized cue round-trips");
    check(!loaded.cues[1].set && !loaded.cues[2].set,
          "unset cue slots remain empty");

    const auto stateFile = onlyStateFile(stateRoot);
    const auto payload = stateFile.loadFileAsString();
    check(!payload.contains(source.getFullPathName()),
          "payload omits raw source path");
    check(!payload.contains(source.getFileName()),
          "payload omits source filename");
    check(!stateFile.getFileName().containsIgnoreCase(source.getFileNameWithoutExtension()),
          "state filename is a non-plaintext source key");

    check(source.appendText("identity-change", false, false, "\n"),
          "source identity can be changed for invalidation fixture");
    TrackHotCueSnapshot stale;
    check(!store.load(source, stale),
          "changed source identity rejects stale hotcues");
    check(!stale.cues[0].set && !stale.cues[7].set,
          "failed load returns a cleared snapshot");
}

void ownerRoundTripRestoresExactCueMetadataTransactionally() {
    TempRoot temp;
    const auto source = makeSource(temp.directory, "owner-roundtrip.wav");
    TrackHotCueStore store(temp.directory.getChildFile("state"));

    broke::Engine sourceEngine;
    broke::PerformanceDeckOwner sourceOwner(sourceEngine, 0);
    broke::BeatGrid grid;
    check(grid.reset(0.5, 120.0), "owner persistence grid initializes");
    check(sourceOwner.setReviewedGrid(grid) == broke::PerformanceDeckOwner::Result::applied,
          "owner persistence grid is reviewed");
    check(sourceOwner.storeHotCueAt(0, 1.30, 20.0, true, 1.0)
              == broke::PerformanceDeckOwner::Result::applied,
          "owner stores quantized cue before persistence");
    check(sourceOwner.storeHotCueAt(5, 7.125, 20.0, false, 4.0)
              == broke::PerformanceDeckOwner::Result::applied,
          "owner stores exact unquantized cue before persistence");

    TrackHotCueSnapshot snapshot;
    snapshot.cues = sourceOwner.hotCueBank();
    check(store.store(source, snapshot), "owner cue bank persists");

    TrackHotCueSnapshot loaded;
    check(store.load(source, loaded), "owner cue bank reloads");

    broke::Engine restoredEngine;
    broke::PerformanceDeckOwner restoredOwner(restoredEngine, 0);
    check(restoredOwner.restoreHotCueBank(loaded.cues, 20.0)
              == broke::PerformanceDeckOwner::Result::applied,
          "loaded cue bank restores transactionally");
    const auto cue0 = restoredOwner.hotCue(0);
    const auto cue5 = restoredOwner.hotCue(5);
    check(cue0.set && cue0.quantized && std::abs(cue0.seconds - 1.5) < 1.0e-9
              && std::abs(cue0.beatStep - 1.0) < 1.0e-12,
          "restore preserves exact quantized cue source time and metadata");
    check(cue5.set && !cue5.quantized && std::abs(cue5.seconds - 7.125) < 1.0e-12
              && std::abs(cue5.beatStep - 4.0) < 1.0e-12,
          "restore preserves exact unquantized cue metadata");
    check(restoredOwner.triggerHotCue(0, 20.0)
              == broke::PerformanceDeckOwner::Result::applied,
          "restored cue remains a functional transport target");
    check(std::abs(restoredEngine.control(0).seek.load() - 0.075) < 1.0e-12,
          "restored cue publishes exact normalized seek");

    auto invalidBank = loaded.cues;
    invalidBank[7] = {true, 25.0, true, 1.0};
    check(restoredOwner.restoreHotCueBank(invalidBank, 20.0)
              == broke::PerformanceDeckOwner::Result::outsideTrack,
          "out-of-track persisted bank is rejected");
    check(restoredOwner.hotCue(0).set
              && std::abs(restoredOwner.hotCue(0).seconds - 1.5) < 1.0e-9
              && restoredOwner.hotCue(5).set,
          "failed restore preserves previous complete in-memory cue bank");
}

void rewritesAreAtomicAndInvalidInputDoesNotClobber() {
    TempRoot temp;
    const auto source = makeSource(temp.directory, "track.flac");
    const auto stateRoot = temp.directory.getChildFile("state");
    TrackHotCueStore store(stateRoot);

    TrackHotCueSnapshot first;
    first.cues[1] = {true, 5.0, false, 1.0};
    check(store.store(source, first), "initial hotcue state stores");

    TrackHotCueSnapshot second;
    second.cues[1] = {true, 6.5, true, 2.0};
    second.cues[6] = {true, 30.0, false, 8.0};
    check(store.store(source, second), "replacement hotcue state stores atomically");

    TrackHotCueSnapshot loaded;
    check(store.load(source, loaded), "replacement state reloads");
    check(std::abs(loaded.cues[1].seconds - 6.5) < 1.0e-12
              && loaded.cues[1].quantized
              && loaded.cues[6].set,
          "latest complete snapshot wins");

    auto invalid = second;
    invalid.cues[2] = {true, std::numeric_limits<double>::infinity(), false, 1.0};
    check(!store.store(source, invalid), "non-finite cue is rejected before write");
    check(store.load(source, loaded) && std::abs(loaded.cues[1].seconds - 6.5) < 1.0e-12,
          "rejected write preserves previous valid state");

    invalid = second;
    invalid.cues[2] = {true, 12.0, true, 0.0};
    check(!store.store(source, invalid), "invalid beat step is rejected before write");
    check(store.load(source, loaded) && loaded.cues[6].set,
          "invalid beat-step write does not clobber state");
}

void corruptUnknownAndOversizedPayloadsFailClosed() {
    TempRoot temp;
    const auto source = makeSource(temp.directory, "track.ogg");
    const auto stateRoot = temp.directory.getChildFile("state");
    TrackHotCueStore store(stateRoot);

    TrackHotCueSnapshot snapshot;
    snapshot.cues[0] = {true, 2.0, true, 1.0};
    check(store.store(source, snapshot), "corruption fixture stores");
    const auto stateFile = onlyStateFile(stateRoot);

    check(stateFile.replaceWithText("schema=999\nfileSize=1\n", false, false, "\n"),
          "future-schema corruption fixture written");
    TrackHotCueSnapshot loaded;
    check(!store.load(source, loaded), "unknown schema fails closed");

    juce::String oversized;
    oversized.preallocateBytes(20 * 1024);
    while (oversized.getNumBytesAsUTF8() <= 17 * 1024) oversized << "0123456789abcdef";
    check(stateFile.replaceWithText(oversized, false, false, "\n"),
          "oversized corruption fixture written");
    check(!store.load(source, loaded), "oversized state fails closed before parse");

    check(store.erase(source), "hotcue state erase succeeds");
    check(!stateFile.existsAsFile(), "erase removes persisted state");
    check(store.erase(source), "erase is idempotent when state is already absent");
}
} // namespace

int main() {
    try {
        roundTripIsSourceBoundAndPrivate();
        ownerRoundTripRestoresExactCueMetadataTransactionally();
        rewritesAreAtomicAndInvalidInputDoesNotClobber();
        corruptUnknownAndOversizedPayloadsFailClosed();
        std::cout << "PerformanceStateStoreTests: " << checks << " checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "PerformanceStateStoreTests failed after " << checks
                  << " checks: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
