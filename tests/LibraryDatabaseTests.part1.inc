// SPDX-License-Identifier: AGPL-3.0-only
#include "app/LibraryDatabase.h"
#include "app/SessionStore.h"

#include <sqlite3.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
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

void writeBytes(const std::filesystem::path& path, std::string_view bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    check(static_cast<bool>(output), "open content-hash fixture");
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(output), "write content-hash fixture");
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
