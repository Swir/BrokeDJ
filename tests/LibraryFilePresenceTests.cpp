// SPDX-License-Identifier: AGPL-3.0-only
#include "app/LibraryDatabase.h"

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
    track.path = path.generic_string();
    track.fileSize = std::filesystem::exists(path)
        ? static_cast<std::int64_t>(std::filesystem::file_size(path)) : 0;
    track.modifiedNs = 1;
    track.title = std::move(title);
    track.artist = "Presence Fixture";
    track.durationSeconds = 1.0;
    return track;
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
    check(afterMove->changed == 1 && afterMove->missing == 2,
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
    check(restored->changed == 1 && restored->missing == 0,
          "returned file clears stale missing flag");
    check(db.search(broke::library::LibraryDatabase::missingSearchDirective,
                    10, &error).empty(),
          "missing review becomes empty after files are available");
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
        testPagedPresenceRefreshAndMetadataRetention();
        testInvalidCursorFailsClosed();
        std::cout << "Library file-presence tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Library file-presence tests failed: " << error.what() << '\n';
        return 1;
    }
}
