// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#include "LibraryDatabase.h"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <system_error>
#include <utility>

namespace broke::library {
namespace {

class Statement final {
public:
    Statement(sqlite3* db, const char* sql) {
        if (db != nullptr)
            result = sqlite3_prepare_v2(db, sql, -1, &statement, nullptr);
    }
    ~Statement() {
        if (statement != nullptr) sqlite3_finalize(statement);
    }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    [[nodiscard]] bool ready() const noexcept { return result == SQLITE_OK && statement != nullptr; }
    [[nodiscard]] int status() const noexcept { return result; }
    [[nodiscard]] sqlite3_stmt* get() const noexcept { return statement; }

private:
    sqlite3_stmt* statement = nullptr;
    int result = SQLITE_MISUSE;
};

void setError(std::string* output, sqlite3* db, std::string_view prefix) {
    if (output == nullptr) return;
    output->assign(prefix);
    if (db != nullptr) {
        const char* message = sqlite3_errmsg(db);
        if (message != nullptr && *message != '\0') {
            if (!output->empty()) output->append(": ");
            output->append(message);
        }
    }
}

void setError(std::string* output, std::string_view message) {
    if (output != nullptr) output->assign(message);
}

[[nodiscard]] std::string pathUtf8(const std::filesystem::path& path) {
    const auto value = path.lexically_normal().generic_u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

[[nodiscard]] std::int64_t nowUnixMs() noexcept {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

[[nodiscard]] bool bindText(sqlite3_stmt* statement, int index, std::string_view value) noexcept {
    return sqlite3_bind_text(statement, index, value.data(), static_cast<int>(value.size()),
                             SQLITE_TRANSIENT) == SQLITE_OK;
}

[[nodiscard]] std::string columnText(sqlite3_stmt* statement, int column) {
    const auto* text = sqlite3_column_text(statement, column);
    if (text == nullptr) return {};
    const auto bytes = sqlite3_column_bytes(statement, column);
    return std::string(reinterpret_cast<const char*>(text), static_cast<std::size_t>(bytes));
}

[[nodiscard]] TrackRecord readTrack(sqlite3_stmt* statement) {
    TrackRecord track;
    track.id = sqlite3_column_int64(statement, 0);
    track.path = columnText(statement, 1);
    track.fileSize = sqlite3_column_int64(statement, 2);
    track.modifiedNs = sqlite3_column_int64(statement, 3);
    track.title = columnText(statement, 4);
    track.artist = columnText(statement, 5);
    track.album = columnText(statement, 6);
    track.durationSeconds = sqlite3_column_double(statement, 7);
    if (sqlite3_column_type(statement, 8) != SQLITE_NULL)
        track.bpm = sqlite3_column_double(statement, 8);
    track.musicalKey = columnText(statement, 9);
    track.contentHash = columnText(statement, 10);
    track.missing = sqlite3_column_int(statement, 11) != 0;
    return track;
}

constexpr const char* trackSelectColumns =
    "t.id,t.path,t.file_size,t.modified_ns,t.title,t.artist,t.album,"
    "t.duration_seconds,t.bpm,t.musical_key,t.content_hash,t.missing";

[[nodiscard]] bool quickCheck(sqlite3* db, std::string* error) {
    Statement statement(db, "PRAGMA quick_check;");
    if (!statement.ready()) {
        setError(error, db, "prepare quick_check");
        return false;
    }
    const int step = sqlite3_step(statement.get());
    if (step != SQLITE_ROW) {
        setError(error, db, "run quick_check");
        return false;
    }
    const std::string result = columnText(statement.get(), 0);
    if (result != "ok") {
        setError(error, std::string("SQLite quick_check failed: ") + result);
        return false;
    }
    return true;
}

[[nodiscard]] bool copyDatabase(sqlite3* destination, sqlite3* source, std::string* error) {
    sqlite3_backup* backup = sqlite3_backup_init(destination, "main", source, "main");
    if (backup == nullptr) {
        setError(error, destination, "initialize SQLite backup");
        return false;
    }

    int step = SQLITE_OK;
    constexpr int maxBusyRetries = 250;
    int busyRetries = 0;
    do {
        step = sqlite3_backup_step(backup, -1);
        if (step == SQLITE_BUSY || step == SQLITE_LOCKED) {
            if (++busyRetries > maxBusyRetries) break;
            sqlite3_sleep(10);
        }
    } while (step == SQLITE_BUSY || step == SQLITE_LOCKED);

    const int finish = sqlite3_backup_finish(backup);
    if (step != SQLITE_DONE || finish != SQLITE_OK) {
        setError(error, destination, "copy SQLite database");
        return false;
    }
    return true;
}

} // namespace

LibraryDatabase::~LibraryDatabase() {
    close();
}

LibraryDatabase::LibraryDatabase(LibraryDatabase&& other) noexcept : db(std::exchange(other.db, nullptr)) {}

LibraryDatabase& LibraryDatabase::operator=(LibraryDatabase&& other) noexcept {
    if (this == &other) return *this;
    close();
    db = std::exchange(other.db, nullptr);
    return *this;
}

bool LibraryDatabase::open(const std::filesystem::path& file, std::string* error) {
    close();
    if (file.empty()) {
        setError(error, "Library database path is empty");
        return false;
    }

    std::error_code filesystemError;
    const auto parent = file.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, filesystemError);
        if (filesystemError) {
            setError(error, "Unable to create library database directory");
            return false;
        }
    }

    const auto path = pathUtf8(file);
    sqlite3* opened = nullptr;
    const int result = sqlite3_open_v2(path.c_str(), &opened,
                                       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
                                           | SQLITE_OPEN_FULLMUTEX,
                                       nullptr);
    if (result != SQLITE_OK || opened == nullptr) {
        setError(error, opened, "open library database");
        if (opened != nullptr) sqlite3_close_v2(opened);
        return false;
    }

    db = opened;
    if (!configure(error) || !migrate(error) || !integrityCheck(error)) {
        close();
        return false;
    }
    return true;
}

void LibraryDatabase::close() noexcept {
    if (db == nullptr) return;
    sqlite3_close_v2(db);
    db = nullptr;
}

bool LibraryDatabase::configure(std::string* error) {
    if (db == nullptr) {
        setError(error, "Library database is not open");
        return false;
    }
    if (sqlite3_busy_timeout(db, 2500) != SQLITE_OK) {
        setError(error, db, "configure SQLite busy timeout");
        return false;
    }
    return exec("PRAGMA foreign_keys=ON;"
                "PRAGMA journal_mode=WAL;"
                "PRAGMA synchronous=NORMAL;",
                error);
}

bool LibraryDatabase::exec(const char* sql, std::string* error) const {
    if (db == nullptr) {
        setError(error, "Library database is not open");
        return false;
    }
    char* message = nullptr;
    const int result = sqlite3_exec(db, sql, nullptr, nullptr, &message);
    if (result == SQLITE_OK) return true;

    if (error != nullptr) {
        error->assign("SQLite statement failed");
        if (message != nullptr && *message != '\0') {
            error->append(": ");
            error->append(message);
        } else {
            error->append(": ");
            error->append(sqlite3_errmsg(db));
        }
    }
    sqlite3_free(message);
    return false;
}

int LibraryDatabase::schemaVersion() const noexcept {
    if (db == nullptr) return -1;
    Statement statement(db, "PRAGMA user_version;");
    if (!statement.ready() || sqlite3_step(statement.get()) != SQLITE_ROW) return -1;
    return sqlite3_column_int(statement.get(), 0);
}

bool LibraryDatabase::migrate(std::string* error) {
    const int initialVersion = schemaVersion();
    if (initialVersion < 0) {
        setError(error, db, "read library schema version");
        return false;
    }
    if (initialVersion > currentSchemaVersion) {
        setError(error, "Library database was created by a newer BrokeDJ schema");
        return false;
    }

    int version = initialVersion;
    if (version < 1) {
        if (!exec("BEGIN IMMEDIATE;", error)) return false;
        const bool ok = exec(
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
            "PRAGMA user_version=1;",
            error);
        if (!ok || !exec("COMMIT;", error)) {
            (void) exec("ROLLBACK;", nullptr);
            return false;
        }
        version = 1;
    }

    if (version < 2) {
        if (!exec("BEGIN IMMEDIATE;", error)) return false;
        const bool ok = exec(
            "ALTER TABLE tracks ADD COLUMN content_hash TEXT NOT NULL DEFAULT '';"
            "ALTER TABLE tracks ADD COLUMN missing INTEGER NOT NULL DEFAULT 0 "
            "CHECK(missing IN (0,1));"
            "CREATE INDEX idx_tracks_artist_title ON tracks(artist COLLATE NOCASE,title COLLATE NOCASE);"
            "CREATE INDEX idx_tracks_content_hash ON tracks(content_hash) WHERE content_hash<>'';"
            "CREATE INDEX idx_history_played_at ON history(played_at_ms DESC);"
            "CREATE INDEX idx_playlist_items_order ON playlist_items(playlist_id,position);"
            "PRAGMA user_version=2;",
            error);
        if (!ok || !exec("COMMIT;", error)) {
            (void) exec("ROLLBACK;", nullptr);
            return false;
        }
        version = 2;
    }

    return version == currentSchemaVersion;
}

std::optional<std::int64_t> LibraryDatabase::upsertTrack(const TrackRecord& track,
                                                         std::string* error) {
    if (db == nullptr || track.path.empty() || track.fileSize < 0
        || !std::isfinite(track.durationSeconds) || track.durationSeconds < 0.0) {
        setError(error, "Invalid library track metadata");
        return std::nullopt;
    }

    Statement statement(db,
        "INSERT INTO tracks(path,file_size,modified_ns,title,artist,album,duration_seconds,bpm,"
        "musical_key,added_at_ms,last_seen_at_ms,content_hash,missing)"
        "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?10,?11,?12)"
        "ON CONFLICT(path) DO UPDATE SET "
        "file_size=excluded.file_size,modified_ns=excluded.modified_ns,title=excluded.title,"
        "artist=excluded.artist,album=excluded.album,duration_seconds=excluded.duration_seconds,"
        "bpm=excluded.bpm,musical_key=excluded.musical_key,last_seen_at_ms=excluded.last_seen_at_ms,"
        "content_hash=excluded.content_hash,missing=excluded.missing;");
    if (!statement.ready()) {
        setError(error, db, "prepare track upsert");
        return std::nullopt;
    }

    const bool bpmValid = track.bpm.has_value() && std::isfinite(*track.bpm) && *track.bpm > 0.0;
    const auto now = nowUnixMs();
    bool bound = bindText(statement.get(), 1, track.path)
        && sqlite3_bind_int64(statement.get(), 2, track.fileSize) == SQLITE_OK
        && sqlite3_bind_int64(statement.get(), 3, track.modifiedNs) == SQLITE_OK
        && bindText(statement.get(), 4, track.title)
        && bindText(statement.get(), 5, track.artist)
        && bindText(statement.get(), 6, track.album)
        && sqlite3_bind_double(statement.get(), 7, track.durationSeconds) == SQLITE_OK;
    if (bpmValid)
        bound = bound && sqlite3_bind_double(statement.get(), 8, *track.bpm) == SQLITE_OK;
    else
        bound = bound && sqlite3_bind_null(statement.get(), 8) == SQLITE_OK;
    bound = bound
        && bindText(statement.get(), 9, track.musicalKey)
        && sqlite3_bind_int64(statement.get(), 10, now) == SQLITE_OK
        && bindText(statement.get(), 11, track.contentHash)
        && sqlite3_bind_int(statement.get(), 12, track.missing ? 1 : 0) == SQLITE_OK;
    if (!bound || sqlite3_step(statement.get()) != SQLITE_DONE) {
        setError(error, db, "upsert library track");
        return std::nullopt;
    }

    Statement lookup(db, "SELECT id FROM tracks WHERE path=?1;");
    if (!lookup.ready() || !bindText(lookup.get(), 1, track.path)
        || sqlite3_step(lookup.get()) != SQLITE_ROW) {
        setError(error, db, "resolve library track id");
        return std::nullopt;
    }
    return sqlite3_column_int64(lookup.get(), 0);
}

bool LibraryDatabase::markMissing(std::int64_t trackId, bool missing, std::string* error) {
    Statement statement(db, "UPDATE tracks SET missing=?1 WHERE id=?2;");
    if (!statement.ready()
        || sqlite3_bind_int(statement.get(), 1, missing ? 1 : 0) != SQLITE_OK
        || sqlite3_bind_int64(statement.get(), 2, trackId) != SQLITE_OK
        || sqlite3_step(statement.get()) != SQLITE_DONE) {
        setError(error, db, "mark library track missing");
        return false;
    }
    if (sqlite3_changes(db) != 1) {
        setError(error, "Track id does not exist");
        return false;
    }
    return true;
}

bool LibraryDatabase::relocateTrack(std::int64_t trackId, const std::filesystem::path& newPath,
                                    std::int64_t fileSize, std::int64_t modifiedNs,
                                    std::string* error) {
    if (newPath.empty() || fileSize < 0) {
        setError(error, "Invalid relocated track metadata");
        return false;
    }
    const auto path = pathUtf8(newPath);
    Statement statement(db,
        "UPDATE tracks SET path=?1,file_size=?2,modified_ns=?3,missing=0,last_seen_at_ms=?4 "
        "WHERE id=?5;");
    if (!statement.ready() || !bindText(statement.get(), 1, path)
        || sqlite3_bind_int64(statement.get(), 2, fileSize) != SQLITE_OK
        || sqlite3_bind_int64(statement.get(), 3, modifiedNs) != SQLITE_OK
        || sqlite3_bind_int64(statement.get(), 4, nowUnixMs()) != SQLITE_OK
        || sqlite3_bind_int64(statement.get(), 5, trackId) != SQLITE_OK
        || sqlite3_step(statement.get()) != SQLITE_DONE) {
        setError(error, db, "relocate library track");
        return false;
    }
    if (sqlite3_changes(db) != 1) {
        setError(error, "Track id does not exist");
        return false;
    }
    return true;
}

std::vector<TrackRecord> LibraryDatabase::search(std::string_view query, std::size_t limit,
                                                 std::string* error) const {
    std::vector<TrackRecord> result;
    if (db == nullptr) {
        setError(error, "Library database is not open");
        return result;
    }
    limit = std::clamp<std::size_t>(limit, 1, 1000);
    const std::string sql = std::string("SELECT DISTINCT ") + trackSelectColumns
        + " FROM tracks t WHERE ?1='' "
          "OR instr(lower(t.title),lower(?1))>0 "
          "OR instr(lower(t.artist),lower(?1))>0 "
          "OR instr(lower(t.album),lower(?1))>0 "
          "OR instr(lower(t.path),lower(?1))>0 "
          "OR EXISTS(SELECT 1 FROM track_tags tt JOIN tags g ON g.id=tt.tag_id "
          "WHERE tt.track_id=t.id AND instr(lower(g.name),lower(?1))>0) "
          "ORDER BY t.missing ASC,t.artist COLLATE NOCASE,t.title COLLATE NOCASE,t.id LIMIT ?2;";
    Statement statement(db, sql.c_str());
    if (!statement.ready() || !bindText(statement.get(), 1, query)
        || sqlite3_bind_int64(statement.get(), 2, static_cast<sqlite3_int64>(limit)) != SQLITE_OK) {
        setError(error, db, "prepare library search");
        return result;
    }
    for (;;) {
        const int step = sqlite3_step(statement.get());
        if (step == SQLITE_DONE) break;
        if (step != SQLITE_ROW) {
            setError(error, db, "search library");
            result.clear();
            break;
        }
        result.push_back(readTrack(statement.get()));
    }
    return result;
}

bool LibraryDatabase::addTag(std::int64_t trackId, std::string_view tag, std::string* error) {
    if (tag.empty()) {
        setError(error, "Tag must not be empty");
        return false;
    }
    Statement trackExists(db, "SELECT 1 FROM tracks WHERE id=?1 LIMIT 1;");
    if (!trackExists.ready()
        || sqlite3_bind_int64(trackExists.get(), 1, trackId) != SQLITE_OK
        || sqlite3_step(trackExists.get()) != SQLITE_ROW) {
        setError(error, "Track does not exist");
        return false;
    }
    if (!exec("BEGIN IMMEDIATE;", error)) return false;

    Statement insertTag(db, "INSERT INTO tags(name) VALUES(?1) ON CONFLICT(name) DO NOTHING;");
    Statement attach(db,
        "INSERT INTO track_tags(track_id,tag_id) "
        "SELECT ?1,id FROM tags WHERE name=?2 COLLATE NOCASE "
        "ON CONFLICT(track_id,tag_id) DO NOTHING;");
    const bool ok = insertTag.ready() && bindText(insertTag.get(), 1, tag)
        && sqlite3_step(insertTag.get()) == SQLITE_DONE
        && attach.ready()
        && sqlite3_bind_int64(attach.get(), 1, trackId) == SQLITE_OK
        && bindText(attach.get(), 2, tag)
        && sqlite3_step(attach.get()) == SQLITE_DONE
        && sqlite3_changes(db) <= 1;
    if (!ok || !exec("COMMIT;", error)) {
        if (!ok) setError(error, db, "attach library tag");
        (void) exec("ROLLBACK;", nullptr);
        return false;
    }
    return true;
}

bool LibraryDatabase::removeTag(std::int64_t trackId, std::string_view tag, std::string* error) {
    Statement statement(db,
        "DELETE FROM track_tags WHERE track_id=?1 AND tag_id IN "
        "(SELECT id FROM tags WHERE name=?2 COLLATE NOCASE);");
    if (!statement.ready()
        || sqlite3_bind_int64(statement.get(), 1, trackId) != SQLITE_OK
        || !bindText(statement.get(), 2, tag)
        || sqlite3_step(statement.get()) != SQLITE_DONE) {
        setError(error, db, "remove library tag");
        return false;
    }
    return true;
}

std::vector<std::string> LibraryDatabase::tagsForTrack(std::int64_t trackId,
                                                       std::string* error) const {
    std::vector<std::string> result;
    Statement statement(db,
        "SELECT g.name FROM tags g JOIN track_tags tt ON tt.tag_id=g.id "
        "WHERE tt.track_id=?1 ORDER BY g.name COLLATE NOCASE;");
    if (!statement.ready() || sqlite3_bind_int64(statement.get(), 1, trackId) != SQLITE_OK) {
        setError(error, db, "prepare track tag query");
        return result;
    }
    for (;;) {
        const int step = sqlite3_step(statement.get());
        if (step == SQLITE_DONE) break;
        if (step != SQLITE_ROW) {
            setError(error, db, "read track tags");
            result.clear();
            break;
        }
        result.push_back(columnText(statement.get(), 0));
    }
    return result;
}

std::optional<std::int64_t> LibraryDatabase::createPlaylist(std::string_view name,
                                                            std::string* error) {
    if (name.empty()) {
        setError(error, "Playlist name must not be empty");
        return std::nullopt;
    }
    Statement insert(db,
        "INSERT INTO playlists(name,created_at_ms) VALUES(?1,?2) "
        "ON CONFLICT(name) DO NOTHING;");
    if (!insert.ready() || !bindText(insert.get(), 1, name)
        || sqlite3_bind_int64(insert.get(), 2, nowUnixMs()) != SQLITE_OK
        || sqlite3_step(insert.get()) != SQLITE_DONE) {
        setError(error, db, "create playlist");
        return std::nullopt;
    }

    Statement lookup(db, "SELECT id FROM playlists WHERE name=?1 COLLATE NOCASE;");
    if (!lookup.ready() || !bindText(lookup.get(), 1, name)
        || sqlite3_step(lookup.get()) != SQLITE_ROW) {
        setError(error, db, "resolve playlist id");
        return std::nullopt;
    }
    return sqlite3_column_int64(lookup.get(), 0);
}

bool LibraryDatabase::addToPlaylist(std::int64_t playlistId, std::int64_t trackId,
                                    std::string* error) {
    Statement statement(db,
        "INSERT INTO playlist_items(playlist_id,track_id,position) "
        "SELECT ?1,?2,COALESCE(MAX(position)+1,0) FROM playlist_items WHERE playlist_id=?1 "
        "ON CONFLICT(playlist_id,track_id) DO NOTHING;");
    if (!statement.ready()
        || sqlite3_bind_int64(statement.get(), 1, playlistId) != SQLITE_OK
        || sqlite3_bind_int64(statement.get(), 2, trackId) != SQLITE_OK
        || sqlite3_step(statement.get()) != SQLITE_DONE) {
        setError(error, db, "add track to playlist");
        return false;
    }
    return true;
}

bool LibraryDatabase::removeFromPlaylist(std::int64_t playlistId, std::int64_t trackId,
                                         std::string* error) {
    Statement statement(db,
        "DELETE FROM playlist_items WHERE playlist_id=?1 AND track_id=?2;");
    if (!statement.ready()
        || sqlite3_bind_int64(statement.get(), 1, playlistId) != SQLITE_OK
        || sqlite3_bind_int64(statement.get(), 2, trackId) != SQLITE_OK
        || sqlite3_step(statement.get()) != SQLITE_DONE) {
        setError(error, db, "remove track from playlist");
        return false;
    }
    return true;
}

std::vector<TrackRecord> LibraryDatabase::playlistTracks(std::int64_t playlistId,
                                                         std::string* error) const {
    std::vector<TrackRecord> result;
    const std::string sql = std::string("SELECT ") + trackSelectColumns
        + " FROM playlist_items pi JOIN tracks t ON t.id=pi.track_id "
          "WHERE pi.playlist_id=?1 ORDER BY pi.position,pi.rowid;";
    Statement statement(db, sql.c_str());
    if (!statement.ready()
        || sqlite3_bind_int64(statement.get(), 1, playlistId) != SQLITE_OK) {
        setError(error, db, "prepare playlist query");
        return result;
    }
    for (;;) {
        const int step = sqlite3_step(statement.get());
        if (step == SQLITE_DONE) break;
        if (step != SQLITE_ROW) {
            setError(error, db, "read playlist");
            result.clear();
            break;
        }
        result.push_back(readTrack(statement.get()));
    }
    return result;
}

bool LibraryDatabase::recordPlay(std::int64_t trackId, std::int64_t playedAtUnixMs,
                                 std::string* error) {
    if (playedAtUnixMs < 0) {
        setError(error, "History timestamp must be non-negative");
        return false;
    }
    Statement statement(db, "INSERT INTO history(track_id,played_at_ms) VALUES(?1,?2);");
    if (!statement.ready()
        || sqlite3_bind_int64(statement.get(), 1, trackId) != SQLITE_OK
        || sqlite3_bind_int64(statement.get(), 2, playedAtUnixMs) != SQLITE_OK
        || sqlite3_step(statement.get()) != SQLITE_DONE) {
        setError(error, db, "record play history");
        return false;
    }
    return true;
}

std::vector<HistoryEntry> LibraryDatabase::recentHistory(std::size_t limit,
                                                         std::string* error) const {
    std::vector<HistoryEntry> result;
    limit = std::clamp<std::size_t>(limit, 1, 1000);
    Statement statement(db,
        "SELECT id,track_id,played_at_ms FROM history "
        "ORDER BY played_at_ms DESC,id DESC LIMIT ?1;");
    if (!statement.ready()
        || sqlite3_bind_int64(statement.get(), 1, static_cast<sqlite3_int64>(limit)) != SQLITE_OK) {
        setError(error, db, "prepare play history query");
        return result;
    }
    for (;;) {
        const int step = sqlite3_step(statement.get());
        if (step == SQLITE_DONE) break;
        if (step != SQLITE_ROW) {
            setError(error, db, "read play history");
            result.clear();
            break;
        }
        result.push_back(HistoryEntry{
            sqlite3_column_int64(statement.get(), 0),
            sqlite3_column_int64(statement.get(), 1),
            sqlite3_column_int64(statement.get(), 2)});
    }
    return result;
}

std::vector<DuplicateGroup> LibraryDatabase::duplicateGroups(std::size_t maxTracks,
                                                             std::string* error) const {
    std::vector<DuplicateGroup> result;
    maxTracks = std::clamp<std::size_t>(maxTracks, 2, 4096);
    const std::string sql = std::string("SELECT ") + trackSelectColumns
        + " FROM tracks t WHERE t.missing=0 AND t.content_hash<>'' "
          "AND t.content_hash IN(SELECT content_hash FROM tracks "
          "WHERE missing=0 AND content_hash<>'' GROUP BY content_hash HAVING COUNT(*)>1) "
          "ORDER BY t.content_hash,t.id LIMIT ?1;";
    Statement statement(db, sql.c_str());
    if (!statement.ready()
        || sqlite3_bind_int64(statement.get(), 1, static_cast<sqlite3_int64>(maxTracks)) != SQLITE_OK) {
        setError(error, db, "prepare duplicate query");
        return result;
    }
    for (;;) {
        const int step = sqlite3_step(statement.get());
        if (step == SQLITE_DONE) break;
        if (step != SQLITE_ROW) {
            setError(error, db, "read duplicate candidates");
            result.clear();
            break;
        }
        auto track = readTrack(statement.get());
        if (result.empty() || result.back().contentHash != track.contentHash)
            result.push_back(DuplicateGroup{track.contentHash, {}});
        result.back().tracks.push_back(std::move(track));
    }
    return result;
}

bool LibraryDatabase::backupTo(const std::filesystem::path& destination,
                               std::string* error) const {
    if (db == nullptr || destination.empty()) {
        setError(error, "Invalid library backup destination");
        return false;
    }
    std::error_code filesystemError;
    const auto parent = destination.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, filesystemError);
        if (filesystemError) {
            setError(error, "Unable to create backup directory");
            return false;
        }
    }

    sqlite3* target = nullptr;
    const auto path = pathUtf8(destination);
    const int opened = sqlite3_open_v2(path.c_str(), &target,
                                       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
                                           | SQLITE_OPEN_FULLMUTEX,
                                       nullptr);
    if (opened != SQLITE_OK || target == nullptr) {
        setError(error, target, "open backup destination");
        if (target != nullptr) sqlite3_close_v2(target);
        return false;
    }

    const bool ok = copyDatabase(target, db, error);
    const int closed = sqlite3_close_v2(target);
    if (ok && closed != SQLITE_OK) {
        setError(error, "close backup destination");
        return false;
    }
    return ok;
}

bool LibraryDatabase::restoreFrom(const std::filesystem::path& source,
                                  std::string* error) {
    if (db == nullptr || source.empty()) {
        setError(error, "Invalid library restore source");
        return false;
    }

    sqlite3* input = nullptr;
    const auto path = pathUtf8(source);
    const int opened = sqlite3_open_v2(path.c_str(), &input,
                                       SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX,
                                       nullptr);
    if (opened != SQLITE_OK || input == nullptr) {
        setError(error, input, "open backup source");
        if (input != nullptr) sqlite3_close_v2(input);
        return false;
    }
    if (!quickCheck(input, error)) {
        sqlite3_close_v2(input);
        return false;
    }
    Statement sourceVersion(input, "PRAGMA user_version;");
    if (!sourceVersion.ready() || sqlite3_step(sourceVersion.get()) != SQLITE_ROW) {
        setError(error, input, "read backup schema version");
        sqlite3_close_v2(input);
        return false;
    }
    const int backupSchemaVersion = sqlite3_column_int(sourceVersion.get(), 0);
    if (backupSchemaVersion > currentSchemaVersion) {
        setError(error, "Backup was created by a newer BrokeDJ schema");
        sqlite3_close_v2(input);
        return false;
    }

    if (!exec("PRAGMA foreign_keys=OFF;", error)) {
        sqlite3_close_v2(input);
        return false;
    }
    const bool copied = copyDatabase(db, input, error);
    const int closed = sqlite3_close_v2(input);
    if (!copied || closed != SQLITE_OK) {
        (void) exec("PRAGMA foreign_keys=ON;", nullptr);
        if (copied) setError(error, "close backup source");
        return false;
    }

    if (!configure(error) || !migrate(error) || !integrityCheck(error)) return false;
    return true;
}

bool LibraryDatabase::integrityCheck(std::string* error) const {
    if (db == nullptr) {
        setError(error, "Library database is not open");
        return false;
    }
    return quickCheck(db, error);
}

} // namespace broke::library
