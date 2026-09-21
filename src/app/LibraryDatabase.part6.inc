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
                                       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
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
                                       SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr);
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
