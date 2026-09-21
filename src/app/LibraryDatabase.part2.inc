    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || ec) return std::string{};
    const auto hash = sha256File(path);
    if (!hash) {
        setError(error, "Unable to read local track for content hashing");
        return std::nullopt;
    }
    return *hash;
}

} // namespace

LibraryDatabase::~LibraryDatabase() { close(); }

LibraryDatabase::LibraryDatabase(LibraryDatabase&& other) noexcept
    : db(std::exchange(other.db, nullptr)) {}

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
                                       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
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
                "PRAGMA synchronous=NORMAL;", error);
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
