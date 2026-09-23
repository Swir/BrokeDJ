// SPDX-License-Identifier: AGPL-3.0-only
#include "app/LibraryDatabase.h"
#include "app/LibraryPresenceAccounting.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct TempDirectory final {
    std::filesystem::path path;
    TempDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path()
            / ("brokedj-file-presence-tests-" + std::to_string(stamp));
        std::filesystem::create_directories(path);
    }
    ~TempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

std::string pathUtf8(const std::filesystem::path& path) {
    const auto value = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

void writeBytes(const std::filesystem::path& path, std::string_view bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    check(static_cast<bool>(output), "open fixture");
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(output), "write fixture");
}

broke::library::TrackRecord makeTrack(const std::filesystem::path& path,
                                      std::string title) {
    broke::library::TrackRecord track;
    track.path = pathUtf8(path);
    track.fileSize = std::filesystem::exists(path)
        ? static_cast<std::int64_t>(std::filesystem::is_regular_file(path)
              ? std::filesystem::file_size(path) : 0)
        : 0;
    track.modifiedNs = 1;
    track.title = std::move(title);
    track.artist = "Presence Fixture";
    track.durationSeconds = 1.0;
    return track;
}

void testConditionalUpdateAccountingFailsClosed() {
    const auto committedMissing = broke::library::accountPresenceConditionalUpdate(true, 1);
    check(committedMissing.changed == 1 && committedMissing.unresolved == 0
              && !committedMissing.discardObservedMissing,
          "one-row guarded update is settled");

    const auto racedMissing = broke::library::accountPresenceConditionalUpdate(true, 0);
    check(racedMissing.changed == 0 && racedMissing.unresolved == 1
              && racedMissing.discardObservedMissing,
          "raced missing observation is unresolved and discarded from settled count");

    const auto racedPresent = broke::library::accountPresenceConditionalUpdate(false, 0);
    check(racedPresent.changed == 0 && racedPresent.unresolved == 1
              && !racedPresent.discardObservedMissing,
          "raced present observation is unresolved without missing correction");

    const auto impossibleMultiRow = broke::library::accountPresenceConditionalUpdate(true, 2);
    check(impossibleMultiRow.changed == 0 && impossibleMultiRow.unresolved == 1
              && impossibleMultiRow.discardObservedMissing,
          "unexpected guarded row count fails closed");
}

void testPagedPresenceRefreshAndMetadataRetention() {
    TempDirectory temp;
    const auto present = temp.path / "music" / "present.wav";
    const auto missing = temp.path / "music" / "missing.wav";
    const auto displaced = temp.path / "relocated" / "present.wav";
    writeBytes(present, "present-content");

    std::string error;
    broke::library::LibraryDatabase db;
    check(db.open(temp.path / "library.sqlite3", &error), "open library");

    const auto presentId = db.upsertTrack(makeTrack(present, "Present"), &error);
    const auto missingId = db.upsertTrack(makeTrack(missing, "Missing"), &error);
    check(presentId && missingId, "insert presence fixtures");
    check(db.addTag(*presentId, "KeepMe", &error), "attach tag before refresh");
    const auto playlist = db.createPlaylist("Presence Playlist", &error);
    check(playlist.has_value(), "create playlist before refresh");
    check(db.addToPlaylist(*playlist, *presentId, &error), "attach playlist before refresh");

    auto firstPage = db.refreshFilePresence(1, 0, &error);
    check(firstPage.has_value(), "first presence page succeeds");
    check(firstPage->scanned == 1 && !firstPage->complete,
          "first presence page is bounded");
    check(firstPage->changed == 0 && firstPage->missing == 0
              && firstPage->unresolved == 0,
          "existing file remains present");

    auto secondPage = db.refreshFilePresence(1, firstPage->nextAfterTrackId, &error);
    check(secondPage.has_value(), "second presence page succeeds");
    check(secondPage->scanned == 1 && secondPage->complete,
          "second presence page reaches end");
    check(secondPage->changed == 1 && secondPage->missing == 1
              && secondPage->unresolved == 0,
          "missing path is discovered without a load attempt");

    auto missingRows = db.search(broke::library::LibraryDatabase::missingSearchDirective,
                                 10, &error);
    check(missingRows.size() == 1 && missingRows.front().id == *missingId,
          "missing review reflects filesystem refresh");

    std::filesystem::create_directories(displaced.parent_path());
    std::filesystem::rename(present, displaced);
    auto afterMove = db.refreshFilePresence(64, 0, &error);
    check(afterMove.has_value(), "refresh after source move succeeds");
    check(afterMove->scanned == 2 && afterMove->complete,
          "full refresh covers fixture library");
    check(afterMove->changed == 1 && afterMove->missing == 2
              && afterMove->unresolved == 0,
          "moved source becomes missing before relocate");

    missingRows = db.search(broke::library::LibraryDatabase::missingSearchDirective,
                            10, &error);
    check(missingRows.size() == 2, "both unavailable paths appear in missing review");
    check(db.tagsForTrack(*presentId, &error).size() == 1,
          "presence refresh preserves tags");
    const auto playlistRows = db.playlistTracks(*playlist, &error);
    check(playlistRows.size() == 1 && playlistRows.front().id == *presentId,
          "presence refresh preserves playlist membership and stable id");

    check(db.relocateTrack(*presentId, displaced,
                           static_cast<std::int64_t>(std::filesystem::file_size(displaced)),
                           2, &error),
          "relocate reconnects moved source");
    missingRows = db.search(broke::library::LibraryDatabase::missingSearchDirective,
                            10, &error);
    check(missingRows.size() == 1 && missingRows.front().id == *missingId,
          "relocate clears moved source missing flag");
    check(db.tagsForTrack(*presentId, &error).size() == 1,
          "relocate after refresh preserves tags");
    check(db.playlistTracks(*playlist, &error).front().id == *presentId,
          "relocate after refresh preserves playlist membership");

    writeBytes(missing, "restored-content");
    auto restored = db.refreshFilePresence(64, 0, &error);
    check(restored.has_value(), "refresh after file return succeeds");
    check(restored->changed == 1 && restored->missing == 0
              && restored->unresolved == 0,
          "returned file clears stale missing flag");
    check(db.search(broke::library::LibraryDatabase::missingSearchDirective,
                    10, &error).empty(),
          "missing review becomes empty after files are available");
}

void testContentHashScopedWitnessVerification() {
    TempDirectory temp;
    const auto primary = temp.path / "fixture" / "primary.wav";
    const auto duplicate = temp.path / "fixture" / "duplicate.wav";
    const auto relocated = temp.path / "relocated" / "primary.wav";
    const auto unrelatedMissing = temp.path / "private-user-track.wav";
    writeBytes(primary, "same-synthetic-fixture-content");
    writeBytes(duplicate, "same-synthetic-fixture-content");

    std::string error;
    broke::library::LibraryDatabase db;
    check(db.open(temp.path / "fixture-witness.sqlite3", &error),
          "open content-hash witness library");

    const auto primaryId = db.upsertTrack(makeTrack(primary, "Fixture Primary"), &error);
    const auto duplicateId = db.upsertTrack(makeTrack(duplicate, "Fixture Duplicate"), &error);
    auto unrelated = makeTrack(unrelatedMissing, "Unrelated");
    unrelated.artist = "Other Library Content";
    const auto unrelatedId = db.upsertTrack(unrelated, &error);
    check(primaryId && duplicateId && unrelatedId, "insert content-hash witness fixtures");

    const auto fixtureRows = db.search("Fixture", 10, &error);
    check(fixtureRows.size() == 2, "fixture search resolves both identical files");
    const auto fixtureHash = fixtureRows.front().contentHash;
    check(fixtureHash.size() == 64 && fixtureRows.back().contentHash == fixtureHash,
          "identical fixture files share one SHA-256 content hash");

    check(db.addTag(*primaryId, "M4-Witness", &error),
          "attach fixture tag for aggregate verification");
    const auto playlist = db.createPlaylist("M4 Witness", &error);
    check(playlist.has_value(), "create fixture playlist");
    check(db.addToPlaylist(*playlist, *primaryId, &error),
          "attach fixture to playlist");
    check(db.recordPlay(*primaryId, 123456789, &error),
          "record fixture history entry");

    const auto before = db.contentHashWorkflowSummary(fixtureHash, &error);
    check(before.has_value(), "read privacy-safe fixture workflow summary");
    check(before->trackCount == 2 && before->missingTrackCount == 0,
          "fixture summary counts duplicate tracks without leaking metadata");
    check(before->historyCount == 1 && before->tagAssociationCount == 1
              && before->playlistMembershipCount == 1,
          "fixture summary observes history/tag/playlist workflow state");

    std::filesystem::create_directories(relocated.parent_path());
    std::filesystem::rename(primary, relocated);

    const auto bounded = db.refreshFilePresenceForContentHash(fixtureHash, 1, &error);
    check(bounded.has_value(), "bounded fixture-only presence refresh succeeds");
    check(bounded->scanned == 1 && !bounded->complete,
          "fixture-only refresh reports truncation instead of scanning without a bound");
    check(bounded->changed == 1 && bounded->missing == 1 && bounded->unresolved == 0,
          "fixture-only refresh marks the moved primary missing");

    const auto moved = db.refreshFilePresenceForContentHash(fixtureHash, 16, &error);
    check(moved.has_value() && moved->complete && moved->scanned == 2,
          "full fixture-only presence refresh reaches both duplicate rows");
    check(moved->missing == 1 && moved->unresolved == 0,
          "fixture-only refresh reports one settled missing row after the move");

    check(db.relocateTrack(*primaryId, relocated,
                           static_cast<std::int64_t>(std::filesystem::file_size(relocated)),
                           2, &error),
          "fixture relocate reconnects the moved primary");

    const auto settled = db.refreshFilePresenceForContentHash(fixtureHash, 16, &error);
    check(settled.has_value() && settled->complete && settled->scanned == 2,
          "post-relocate fixture refresh is complete");
    check(settled->missing == 0 && settled->unresolved == 0,
          "post-relocate fixture refresh has zero missing/unresolved results");

    const auto after = db.contentHashWorkflowSummary(fixtureHash, &error);
    check(after.has_value(), "read post-relocate fixture workflow summary");
    check(after->trackCount == 2 && after->missingTrackCount == 0,
          "fixture duplicate remains connected after relocate");
    check(after->historyCount == 1 && after->tagAssociationCount == 1
              && after->playlistMembershipCount == 1,
          "fixture workflow metadata survives targeted presence and relocate operations");

    check(db.search(broke::library::LibraryDatabase::missingSearchDirective,
                    10, &error).empty(),
          "fixture-only refresh does not mutate unrelated user-track missing flags");

    check(!db.refreshFilePresenceForContentHash("ABC", 16, &error).has_value(),
          "malformed witness hash is rejected");
    check(error.find("SHA-256") != std::string::npos,
          "malformed witness hash reports the expected validation error");
}

void testUnicodeAndNonRegularPathClassification() {
    TempDirectory temp;
    const auto unicodeFile = temp.path / std::filesystem::path(u8"muzyka-zażółć.wav");
    const auto directoryPath = temp.path / "not-a-track.wav";
    writeBytes(unicodeFile, "unicode-content");
    std::filesystem::create_directories(directoryPath);

    std::string error;
    broke::library::LibraryDatabase db;
    check(db.open(temp.path / "unicode.sqlite3", &error), "open unicode library");

    const auto unicodeId = db.upsertTrack(makeTrack(unicodeFile, "Unicode"), &error);
    const auto directoryId = db.upsertTrack(makeTrack(directoryPath, "Directory"), &error);
    check(unicodeId && directoryId, "insert unicode/directory fixtures");

    auto first = db.refreshFilePresence(64, 0, &error);
    check(first.has_value(), "unicode/directory refresh succeeds");
    check(first->scanned == 2 && first->complete, "unicode/directory refresh is complete");
    check(first->changed == 1 && first->missing == 1 && first->unresolved == 0,
          "unicode regular file stays present while directory is marked missing");

    const auto missingRows = db.search(broke::library::LibraryDatabase::missingSearchDirective,
                                       10, &error);
    check(missingRows.size() == 1 && missingRows.front().id == *directoryId,
          "non-regular library path is reviewable as missing");

    std::filesystem::remove_all(directoryPath);
    writeBytes(directoryPath, "now-regular-content");
    auto repaired = db.refreshFilePresence(64, 0, &error);
    check(repaired.has_value(), "refresh after directory replacement succeeds");
    check(repaired->changed == 1 && repaired->missing == 0 && repaired->unresolved == 0,
          "regular replacement clears non-regular missing state");

    const auto unicodeRows = db.search("Unicode", 10, &error);
    check(unicodeRows.size() == 1 && unicodeRows.front().id == *unicodeId,
          "unicode path remains searchable after reconciliation");
}

void testPageSizeClampsWithoutUnboundedScan() {
    TempDirectory temp;
    writeBytes(temp.path / "one.wav", "one");
    writeBytes(temp.path / "two.wav", "two");

    std::string error;
    broke::library::LibraryDatabase db;
    check(db.open(temp.path / "clamp.sqlite3", &error), "open clamp library");
    check(db.upsertTrack(makeTrack(temp.path / "one.wav", "One"), &error).has_value(),
          "insert first clamp fixture");
    check(db.upsertTrack(makeTrack(temp.path / "two.wav", "Two"), &error).has_value(),
          "insert second clamp fixture");

    const auto page = db.refreshFilePresence(0, 0, &error);
    check(page.has_value(), "zero page request clamps to bounded minimum");
    check(page->scanned == 1 && !page->complete && page->nextAfterTrackId > 0,
          "zero page request does not turn into an unbounded scan");
    check(page->changed == 0 && page->missing == 0 && page->unresolved == 0,
          "clamped page preserves valid present state");
}

void testInvalidCursorFailsClosed() {
    TempDirectory temp;
    std::string error;
    broke::library::LibraryDatabase db;
    check(db.open(temp.path / "library.sqlite3", &error), "open cursor test library");
    check(!db.refreshFilePresence(32, -1, &error).has_value(),
          "negative cursor rejected");
    check(error.find("non-negative") != std::string::npos,
          "negative cursor reports bounded validation error");
}

} // namespace

int main() {
    try {
        testConditionalUpdateAccountingFailsClosed();
        testPagedPresenceRefreshAndMetadataRetention();
        testContentHashScopedWitnessVerification();
        testUnicodeAndNonRegularPathClassification();
        testPageSizeClampsWithoutUnboundedScan();
        testInvalidCursorFailsClosed();
        std::cout << "Library file-presence tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Library file-presence tests failed: " << error.what() << '\n';
        return 1;
    }
}
