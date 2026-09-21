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

    std::string sql;
    if (query == duplicateSearchDirective) {
        sql = std::string("SELECT ") + trackSelectColumns
            + " FROM tracks t WHERE t.missing=0 AND t.content_hash<>'' "
              "AND t.content_hash IN(SELECT content_hash FROM tracks WHERE missing=0 "
              "AND content_hash<>'' GROUP BY content_hash HAVING COUNT(*)>1) "
              "ORDER BY t.content_hash,t.artist COLLATE NOCASE,t.title COLLATE NOCASE,t.id LIMIT ?1;";
        Statement statement(db, sql.c_str());
        if (!statement.ready()
            || sqlite3_bind_int64(statement.get(), 1, static_cast<sqlite3_int64>(limit)) != SQLITE_OK) {
            setError(error, db, "prepare duplicate library search");
            return result;
        }
        for (;;) {
            const int step = sqlite3_step(statement.get());
            if (step == SQLITE_DONE) break;
            if (step != SQLITE_ROW) {
                setError(error, db, "search duplicate library tracks");
                result.clear();
                break;
            }
            result.push_back(readTrack(statement.get()));
        }
        return result;
    }

    if (query == missingSearchDirective) {
        sql = std::string("SELECT ") + trackSelectColumns
            + " FROM tracks t WHERE t.missing=1 "
              "ORDER BY t.artist COLLATE NOCASE,t.title COLLATE NOCASE,t.id LIMIT ?1;";
        Statement statement(db, sql.c_str());
        if (!statement.ready()
            || sqlite3_bind_int64(statement.get(), 1, static_cast<sqlite3_int64>(limit)) != SQLITE_OK) {
            setError(error, db, "prepare missing-track library search");
            return result;
        }
        for (;;) {
            const int step = sqlite3_step(statement.get());
            if (step == SQLITE_DONE) break;
            if (step != SQLITE_ROW) {
                setError(error, db, "search missing library tracks");
                result.clear();
                break;
            }
            result.push_back(readTrack(statement.get()));
        }
        return result;
    }

    sql = std::string("SELECT DISTINCT ") + trackSelectColumns
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
