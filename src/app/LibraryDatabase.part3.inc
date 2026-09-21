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
            "PRAGMA user_version=1;", error);
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
            "ALTER TABLE tracks ADD COLUMN missing INTEGER NOT NULL DEFAULT 0 CHECK(missing IN (0,1));"
            "CREATE INDEX idx_tracks_artist_title ON tracks(artist COLLATE NOCASE,title COLLATE NOCASE);"
            "CREATE INDEX idx_tracks_content_hash ON tracks(content_hash) WHERE content_hash<>'';"
            "CREATE INDEX idx_history_played_at ON history(played_at_ms DESC);"
            "CREATE INDEX idx_playlist_items_order ON playlist_items(playlist_id,position);"
            "PRAGMA user_version=2;", error);
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

    const auto resolvedHash = populateHashWhenLocalFileExists(track, error);
    if (!resolvedHash) return std::nullopt;

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
    bound = bound && bindText(statement.get(), 9, track.musicalKey)
        && sqlite3_bind_int64(statement.get(), 10, now) == SQLITE_OK
        && bindText(statement.get(), 11, *resolvedHash)
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
    if (db == nullptr || newPath.empty() || fileSize < 0) {
        setError(error, "Invalid relocated track metadata");
        return false;
    }
    std::error_code ec;
    if (!std::filesystem::is_regular_file(newPath, ec) || ec) {
        setError(error, "Relocated track file does not exist");
        return false;
    }
    const auto hash = sha256File(newPath);
    if (!hash) {
        setError(error, "Unable to read relocated track for content hashing");
        return false;
    }

    const auto path = pathUtf8(newPath);
    Statement statement(db,
        "UPDATE tracks SET path=?1,file_size=?2,modified_ns=?3,content_hash=?4,missing=0,last_seen_at_ms=?5 "
        "WHERE id=?6;");
    if (!statement.ready() || !bindText(statement.get(), 1, path)
        || sqlite3_bind_int64(statement.get(), 2, fileSize) != SQLITE_OK
        || sqlite3_bind_int64(statement.get(), 3, modifiedNs) != SQLITE_OK
        || !bindText(statement.get(), 4, *hash)
        || sqlite3_bind_int64(statement.get(), 5, nowUnixMs()) != SQLITE_OK
        || sqlite3_bind_int64(statement.get(), 6, trackId) != SQLITE_OK
        || sqlite3_step(statement.get()) != SQLITE_DONE) {
        setError(error, db, "relocate library track");
        return false;
    }
    if (sqlite3_changes(db) != 1) {
        setError(error, "Track id does not exist");
        return false;
