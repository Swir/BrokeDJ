// SPDX-License-Identifier: AGPL-3.0-only
#include "app/LibraryDatabase.h"
#include "app/LibraryRuntimeStore.h"

#include <sqlite3.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct TempDirectory final {
    std::filesystem::path path;
    TempDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path()
            / ("brokedj-runtime-store-tests-" + std::to_string(stamp));
        std::filesystem::create_directories(path);
    }
    ~TempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

std::string pathUtf8(const std::filesystem::path& path) {
    const auto value = path.lexically_normal().generic_u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

void writeBytes(const std::filesystem::path& path, std::string_view bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    check(static_cast<bool>(output), "open runtime-store fixture");
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(output), "write runtime-store fixture");
}

broke::library::TrackRecord track(const std::filesystem::path& path,
                                  std::string title,
                                  std::string hash) {
    broke::library::TrackRecord value;
    value.path = pathUtf8(path);
    std::error_code error;
    if (std::filesystem::is_regular_file(path, error) && !error)
        value.fileSize = static_cast<std::int64_t>(std::filesystem::file_size(path));
    else
        value.fileSize = 123;
    value.modifiedNs = 42;
    value.contentHash = std::move(hash);
    value.title = std::move(title);
    value.artist = "Runtime Fixture";
    value.album = "M4";
    value.durationSeconds = 180.0;
    return value;
}

void clearContentHash(const std::filesystem::path& databaseFile, std::int64_t trackId) {
    sqlite3* handle = nullptr;
    const auto utf8 = pathUtf8(databaseFile);
    check(sqlite3_open_v2(utf8.c_str(), &handle,
                          SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr) == SQLITE_OK,
          "open sqlite fixture for legacy hash reset");
    sqlite3_stmt* statement = nullptr;
    check(sqlite3_prepare_v2(handle,
                             "UPDATE tracks SET content_hash='' WHERE id=?1;",
                             -1, &statement, nullptr) == SQLITE_OK,
          "prepare legacy hash reset");
    check(sqlite3_bind_int64(statement, 1, trackId) == SQLITE_OK,
          "bind legacy hash reset");
    check(sqlite3_step(statement) == SQLITE_DONE,
          "execute legacy hash reset");
    check(sqlite3_changes(handle) == 1, "legacy hash reset changed one row");
    sqlite3_finalize(statement);
    check(sqlite3_close_v2(handle) == SQLITE_OK, "close legacy hash reset database");
}

void testJoinedPlaybackHistory() {
    TempDirectory temp;
    const auto databaseFile = temp.path / "history.sqlite3";
    std::string error;
    broke::library::LibraryDatabase db;
    check(db.open(databaseFile, &error), "open runtime history database");

    const auto alphaPath = temp.path / "virtual-alpha.wav";
    const auto betaPath = temp.path / "virtual-beta.wav";
    const auto alphaId = db.upsertTrack(track(alphaPath, "Alpha", "hash-alpha"), &error);
    const auto betaId = db.upsertTrack(track(betaPath, "Beta", "hash-beta"), &error);
    check(alphaId && betaId, "insert runtime history tracks");
    check(db.recordPlay(*alphaId, 1000, &error), "record alpha history start");
    check(db.recordPlay(*betaId, 2000, &error), "record beta history start");
    check(db.recordPlay(*alphaId, 3000, &error), "record second alpha history start");

    const auto rows = broke::library::runtime::recentPlayedTracks(databaseFile, 10, &error);
    check(error.empty(), "joined playback history query has no error");
    check(rows.size() == 3, "joined playback history preserves repeated plays");
    check(rows[0].track.id == *alphaId && rows[0].playedAtUnixMs == 3000,
          "history newest row resolves track metadata");
    check(rows[1].track.id == *betaId && rows[1].playedAtUnixMs == 2000,
          "history second row resolves beta metadata");
    check(rows[2].track.id == *alphaId && rows[2].playedAtUnixMs == 1000,
          "history oldest row remains visible");

    const auto exact = broke::library::runtime::findTrackByPath(
        databaseFile, pathUtf8(betaPath), &error);
    check(error.empty() && exact && exact->id == *betaId && exact->title == "Beta",
          "exact path lookup resolves runtime playback identity");
    const auto missing = broke::library::runtime::findTrackByPath(
        databaseFile, "does/not/exist.wav", &error);
    check(error.empty() && !missing, "exact path lookup cleanly reports no row");
}

void testBoundedLegacyContentHashBackfill() {
    TempDirectory temp;
    const auto databaseFile = temp.path / "backfill.sqlite3";
    std::string error;
    broke::library::LibraryDatabase db;
    check(db.open(databaseFile, &error), "open runtime backfill database");

    const auto hashable = temp.path / "music" / "hashable.wav";
    writeBytes(hashable, "abc");
    const auto hashableId = db.upsertTrack(track(hashable, "Hashable", "legacy-placeholder"), &error);
    check(hashableId.has_value(), "insert hashable legacy row");
    clearContentHash(databaseFile, *hashableId);

    const auto missingPath = temp.path / "music" / "gone.wav";
    auto missingRecord = track(missingPath, "Gone", "");
    const auto missingId = db.upsertTrack(missingRecord, &error);
    check(missingId.has_value(), "insert missing legacy row");

    std::atomic<bool> cancelled{false};
    auto first = broke::library::runtime::backfillContentHashes(
        databaseFile, 1, &cancelled, &error);
    check(error.empty(), "bounded first backfill has no database error");
    check(first.scanned == 1 && first.hashed == 1 && first.markedMissing == 0,
          "backfill limit hashes only first candidate");

    constexpr std::string_view abcSha256 =
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    const auto hashed = broke::library::runtime::findTrackByPath(
        databaseFile, pathUtf8(hashable), &error);
    check(hashed && hashed->contentHash == abcSha256,
          "legacy content hash matches SHA-256 reference vector");

    auto second = broke::library::runtime::backfillContentHashes(
        databaseFile, 8, &cancelled, &error);
    check(error.empty(), "missing-file backfill has no database error");
    check(second.scanned == 1 && second.markedMissing == 1,
          "backfill marks disconnected legacy source without touching music");
    const auto missingRows = db.search(
        broke::library::LibraryDatabase::missingSearchDirective, 10, &error);
    check(missingRows.size() == 1 && missingRows.front().id == *missingId,
          "backfill missing mark is visible through library review");

    const auto cancelFile = temp.path / "music" / "cancel.wav";
    writeBytes(cancelFile, "cancel-me");
    const auto cancelId = db.upsertTrack(track(cancelFile, "Cancel", "legacy-placeholder"), &error);
    check(cancelId.has_value(), "insert cancellation row");
    clearContentHash(databaseFile, *cancelId);

    cancelled.store(true, std::memory_order_release);
    const auto stopped = broke::library::runtime::backfillContentHashes(
        databaseFile, 8, &cancelled, &error);
    check(stopped.cancelled && stopped.hashed == 0,
          "pre-cancelled backfill stops before file hashing");
    auto cancelledRow = broke::library::runtime::findTrackByPath(
        databaseFile, pathUtf8(cancelFile), &error);
    check(cancelledRow && cancelledRow->contentHash.empty(),
          "cancelled backfill leaves legacy identity unpublished");

    cancelled.store(false, std::memory_order_release);
    const auto resumed = broke::library::runtime::backfillContentHashes(
        databaseFile, 8, &cancelled, &error);
    check(resumed.hashed == 1 && !resumed.cancelled,
          "backfill resumes on a later bounded pass");
}

} // namespace

int main() {
    try {
        testJoinedPlaybackHistory();
        testBoundedLegacyContentHashBackfill();
        std::cout << "Library runtime history/backfill tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Library runtime history/backfill tests failed: " << exception.what() << '\n';
        return 1;
    }
}
