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
        "INSERT INTO playlists(name,created_at_ms) VALUES(?1,?2) ON CONFLICT(name) DO NOTHING;");
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
        "SELECT id,track_id,played_at_ms FROM history ORDER BY played_at_ms DESC,id DESC LIMIT ?1;");
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
        result.push_back(HistoryEntry{sqlite3_column_int64(statement.get(), 0),
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
