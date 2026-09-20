// SPDX-License-Identifier: AGPL-3.0-only
#include "app/LibraryDatabase.h"

#include <sqlite3.h>

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
            / ("brokedj-library-tests-" + std::to_string(stamp));
        std::filesystem::create_directories(path);
    }
    ~TempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

broke::library::TrackRecord track(std::string path, std::string title,
                                  std::string artist, std::string hash) {
    broke::library::TrackRecord value;
    value.path = std::move(path);
    value.fileSize = 123456;
    value.modifiedNs = 987654321;
    value.contentHash = std::move(hash);
    value.title = std::move(title);
    value.artist = std::move(artist);
    value.album = "Fixture Album";
    value.durationSeconds = 180.0;
    value.bpm = 140.0;
    value.musicalKey = "8A";
    return value;
}

void createVersionOneDatabase(const std::filesystem::path& path) {
    sqlite3* db = nullptr;
    check(sqlite3_open_v2(path.string().c_str(), &db,
                          SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK,
          "create v1 fixture");
    const char* sql =
        "PRAGMA foreign_keys=ON;"
        "CREATE TABLE tracks("
        "id INTEGER PRIMARY KEY,"
        "path TEXT NOT NULL UNIQUE,"
        "file_size INTEGER NOT NULL CHECK(file_size>=0),"
        "modified_ns INTEGER NOT NULL,"
        "title TEXT NOT NULL DEFAULT '',"
        "artist TEXT NOT NULL DEFAULT '',"
        "album TEXT NOT NULL DEFAULT '',"
        "duration_seconds REAL NOT NULL DEFAULT 0 CHECK(duration_seconds>=0),"
        "bpm REAL,"
        "musical_key TEXT NOT NULL DEFAULT '',"
        "added_at_ms INTEGER NOT NULL,"
        "last_seen_at_ms INTEGER NOT NULL"
        ");"
        "CREATE TABLE tags("
        "id INTEGER PRIMARY KEY,"
        "name TEXT NOT NULL COLLATE NOCASE UNIQUE CHECK(length(trim(name))>0)"
        ");"
        "CREATE TABLE track_tags("
        "track_id INTEGER NOT NULL REFERENCES tracks(id) ON DELETE CASCADE,"
        "tag_id INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,"
        "PRIMARY KEY(track_id,tag_id)"
        ");"
        "CREATE TABLE playlists("
        "id INTEGER PRIMARY KEY,"
        "name TEXT NOT NULL COLLATE NOCASE UNIQUE CHECK(length(trim(name))>0),"
        "created_at_ms INTEGER NOT NULL"
        ");"
        "CREATE TABLE playlist_items("
        "playlist_id INTEGER NOT NULL REFERENCES playlists(id) ON DELETE CASCADE,"
        "track_id INTEGER NOT NULL REFERENCES tracks(id) ON DELETE CASCADE,"
        "position INTEGER NOT NULL CHECK(position>=0),"
        "PRIMARY KEY(playlist_id,track_id),"
        "UNIQUE(playlist_id,position)"
        ");"
        "CREATE TABLE history("
        "id INTEGER PRIMARY KEY,"
        "track_id INTEGER NOT NULL REFERENCES tracks(id) ON DELETE CASCADE,"
        "played_at_ms INTEGER NOT NULL"
        ");"
        "INSERT INTO tracks(path,file_size,modified_ns,title,artist,album,duration_seconds,bpm,"
        "musical_key,added_at_ms,last_seen_at_ms)"
        "VALUES('legacy.wav',42,11,'Legacy','Fixture','','60',128,'7A',1,1);"
        "PRAGMA user_version=1;";
    char* error = nullptr;
    const int result = sqlite3_exec(db, sql, nullptr, nullptr, &error);
    if (result != SQLITE_OK) {
        const std::string message = error != nullptr ? error : "unknown sqlite error";
        sqlite3_free(error);
        sqlite3_close_v2(db);
        throw std::runtime_error("create v1 schema: " + message);
    }
    check(sqlite3_close_v2(db) == SQLITE_OK, "close v1 fixture");
}

void createFutureDatabase(const std::filesystem::path& path) {
    sqlite3* db = nullptr;
    check(sqlite3_open_v2(path.string().c_str(), &db,
                          SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK,
          "create future fixture");
    check(sqlite3_exec(db, "PRAGMA user_version=999;", nullptr, nullptr, nullptr) == SQLITE_OK,
          "set future version");
    check(sqlite3_close_v2(db) == SQLITE_OK, "close future fixture");
}

void testLibraryWorkflow() {
    TempDirectory temp;
    std::string error;
    broke::library::LibraryDatabase db;
    check(db.open(temp.path / "library.sqlite3", &error), "open fresh library");
    check(db.schemaVersion() == broke::library::LibraryDatabase::currentSchemaVersion,
          "fresh library migrated to current schema");
    check(db.integrityCheck(&error), "fresh library integrity");

    auto alpha = track("music/Alpha.wav", "Alpha", "Alice", "sha256-A");
    auto beta = track("music/Beta.wav", "Beta", "Bob", "sha256-B");
    auto alphaCopy = track("music/Alpha Copy.wav", "Alpha Copy", "Alice", "sha256-A");

    const auto alphaId = db.upsertTrack(alpha, &error);
    const auto betaId = db.upsertTrack(beta, &error);
    const auto alphaCopyId = db.upsertTrack(alphaCopy, &error);
    check(alphaId && betaId && alphaCopyId, "insert tracks");

    alpha.album = "Updated Album";
    const auto alphaIdAgain = db.upsertTrack(alpha, &error);
    check(alphaIdAgain && *alphaIdAgain == *alphaId, "upsert keeps stable track id");

    check(db.addTag(*alphaId, "Techno", &error), "add first tag");
    check(db.addTag(*alphaId, "Peak Time", &error), "add second tag");
    check(db.addTag(*alphaId, "TECHNO", &error), "case-insensitive tag remains idempotent");
    const auto tags = db.tagsForTrack(*alphaId, &error);
    check(tags.size() == 2, "tag set remains unique");

    const auto tagSearch = db.search("peak", 20, &error);
    check(tagSearch.size() == 1 && tagSearch.front().id == *alphaId,
          "search resolves tag text");
    const auto artistSearch = db.search("alice", 20, &error);
    check(artistSearch.size() == 2, "search resolves artist text");

    const auto playlist = db.createPlaylist("Tonight", &error);
    check(playlist.has_value(), "create playlist");
    check(db.addToPlaylist(*playlist, *alphaId, &error), "playlist add alpha");
    check(db.addToPlaylist(*playlist, *betaId, &error), "playlist add beta");
    check(db.addToPlaylist(*playlist, *alphaId, &error), "playlist duplicate is idempotent");
    auto playlistTracks = db.playlistTracks(*playlist, &error);
    check(playlistTracks.size() == 2 && playlistTracks[0].id == *alphaId
              && playlistTracks[1].id == *betaId,
          "playlist keeps insertion order");
    check(db.removeFromPlaylist(*playlist, *betaId, &error), "playlist remove");
    playlistTracks = db.playlistTracks(*playlist, &error);
    check(playlistTracks.size() == 1 && playlistTracks.front().id == *alphaId,
          "playlist remove preserves remaining item");

    check(db.recordPlay(*alphaId, 1000, &error), "history alpha");
    check(db.recordPlay(*betaId, 2000, &error), "history beta");
    const auto history = db.recentHistory(10, &error);
    check(history.size() == 2 && history.front().trackId == *betaId
              && history.back().trackId == *alphaId,
          "history sorts newest first");

    auto duplicates = db.duplicateGroups(32, &error);
    check(duplicates.size() == 1 && duplicates.front().contentHash == "sha256-A"
              && duplicates.front().tracks.size() == 2,
          "content hash finds duplicate group");

    check(db.markMissing(*alphaCopyId, true, &error), "mark duplicate missing");
    duplicates = db.duplicateGroups(32, &error);
    check(duplicates.empty(), "missing file excluded from duplicate candidates");
    check(db.markMissing(*alphaCopyId, false, &error), "restore duplicate presence");

    check(db.markMissing(*alphaId, true, &error), "mark source missing");
    const auto relocatedPath = temp.path / "moved" / "Alpha.wav";
    check(db.relocateTrack(*alphaId, relocatedPath, 654321, 123456789, &error),
          "relocate source");
    const auto relocated = db.search("Alpha.wav", 20, &error);
    bool relocatedFound = false;
    for (const auto& item : relocated) {
        if (item.id == *alphaId) {
            relocatedFound = item.path.find("moved/Alpha.wav") != std::string::npos
                && !item.missing && item.fileSize == 654321;
        }
    }
    check(relocatedFound, "relocation preserves id and updates path identity");
    check(db.tagsForTrack(*alphaId, &error).size() == 2,
          "relocation preserves tag relationships");
    check(db.playlistTracks(*playlist, &error).front().id == *alphaId,
          "relocation preserves playlist relationships");

    const auto backup = temp.path / "backup.sqlite3";
    check(db.backupTo(backup, &error), "backup library");
    auto afterBackup = track("music/After Backup.wav", "After Backup", "Carol", "sha256-C");
    check(db.upsertTrack(afterBackup, &error).has_value(), "mutate after backup");
    check(db.search("", 100, &error).size() == 4, "post-backup mutation visible");
    check(db.restoreFrom(backup, &error), "restore library");
    check(db.search("", 100, &error).size() == 3, "restore returns backup snapshot");
    check(db.integrityCheck(&error), "restored database integrity");

    const auto corrupt = temp.path / "corrupt.sqlite3";
    {
        std::ofstream output(corrupt, std::ios::binary);
        output << "not a sqlite database";
    }
    check(!db.restoreFrom(corrupt, &error), "corrupt restore rejected before replacement");
    check(db.search("", 100, nullptr).size() == 3, "failed restore leaves live database intact");

    const auto futureBackup = temp.path / "future-backup.sqlite3";
    createFutureDatabase(futureBackup);
    check(!db.restoreFrom(futureBackup, &error), "future backup schema rejected before replacement");
    check(db.search("", 100, nullptr).size() == 3,
          "future backup rejection leaves live database intact");
}

void testMigration() {
    TempDirectory temp;
    const auto path = temp.path / "legacy.sqlite3";
    createVersionOneDatabase(path);

    std::string error;
    broke::library::LibraryDatabase db;
    check(db.open(path, &error), "open legacy database");
    check(db.schemaVersion() == broke::library::LibraryDatabase::currentSchemaVersion,
          "legacy database migrated");
    const auto tracks = db.search("Legacy", 10, &error);
    check(tracks.size() == 1, "migration preserves track");
    check(tracks.front().contentHash.empty() && !tracks.front().missing,
          "migration applies v2 defaults");
    check(db.integrityCheck(&error), "migrated database integrity");
}

void testFutureSchemaRejected() {
    TempDirectory temp;
    const auto path = temp.path / "future.sqlite3";
    createFutureDatabase(path);

    std::string error;
    broke::library::LibraryDatabase db;
    check(!db.open(path, &error), "future schema must fail closed");
    check(!db.isOpen(), "failed future schema does not remain open");
}

} // namespace

int main() {
    try {
        testLibraryWorkflow();
        testMigration();
        testFutureSchemaRejected();
        std::cout << "LibraryDatabase tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "LibraryDatabase tests failed: " << error.what() << '\n';
        return 1;
    }
}
