// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#include "LibraryDatabase.h"

#include <sqlite3.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace broke::library {
namespace {

class PresenceStatement final {
public:
    PresenceStatement(sqlite3* db, const char* sql) {
        if (db != nullptr) result = sqlite3_prepare_v2(db, sql, -1, &statement, nullptr);
    }
    ~PresenceStatement() { if (statement != nullptr) sqlite3_finalize(statement); }
    PresenceStatement(const PresenceStatement&) = delete;
    PresenceStatement& operator=(const PresenceStatement&) = delete;

    [[nodiscard]] bool ready() const noexcept {
        return result == SQLITE_OK && statement != nullptr;
    }
    [[nodiscard]] sqlite3_stmt* get() const noexcept { return statement; }

private:
    sqlite3_stmt* statement = nullptr;
    int result = SQLITE_MISUSE;
};

void setPresenceError(std::string* output, sqlite3* db, std::string_view prefix) {
    if (output == nullptr) return;
    output->assign(prefix);
    if (db == nullptr) return;
    const char* message = sqlite3_errmsg(db);
    if (message != nullptr && *message != '\0') {
        output->append(": ");
        output->append(message);
    }
}

void setPresenceError(std::string* output, std::string_view message) {
    if (output != nullptr) output->assign(message);
}

[[nodiscard]] bool bindPresenceText(sqlite3_stmt* statement, int index,
                                    std::string_view value) noexcept {
    return sqlite3_bind_text(statement, index, value.data(), static_cast<int>(value.size()),
                             SQLITE_TRANSIENT) == SQLITE_OK;
}

[[nodiscard]] std::string presenceColumnText(sqlite3_stmt* statement, int column) {
    const auto* text = sqlite3_column_text(statement, column);
    if (text == nullptr) return {};
    const auto bytes = sqlite3_column_bytes(statement, column);
    return std::string(reinterpret_cast<const char*>(text), static_cast<std::size_t>(bytes));
}

[[nodiscard]] std::filesystem::path presencePathFromUtf8(std::string_view value) {
    std::u8string utf8;
    utf8.reserve(value.size());
    for (const auto ch : value)
        utf8.push_back(static_cast<char8_t>(static_cast<unsigned char>(ch)));
    return std::filesystem::path(utf8);
}

[[nodiscard]] bool presenceExec(sqlite3* db, const char* sql, std::string* error) {
    char* message = nullptr;
    const int result = sqlite3_exec(db, sql, nullptr, nullptr, &message);
    if (result == SQLITE_OK) return true;
    if (error != nullptr) {
        error->assign("SQLite file-presence refresh failed");
        if (message != nullptr && *message != '\0') {
            error->append(": ");
            error->append(message);
        } else if (db != nullptr) {
            error->append(": ");
            error->append(sqlite3_errmsg(db));
        }
    }
    sqlite3_free(message);
    return false;
}

struct PresenceSnapshot final {
    std::int64_t id = -1;
    std::string path;
    bool missing = false;
};

struct PresenceDecision final {
    PresenceSnapshot snapshot;
    bool missingNow = false;
};

} // namespace

std::optional<FilePresenceRefreshResult> LibraryDatabase::refreshFilePresence(
    std::size_t maxTracks, std::int64_t afterTrackId, std::string* error) {
    if (error != nullptr) error->clear();
    if (db == nullptr) {
        setPresenceError(error, "Library database is not open");
        return std::nullopt;
    }
    if (afterTrackId < 0) {
        setPresenceError(error, "File-presence cursor must be non-negative");
        return std::nullopt;
    }

    maxTracks = std::clamp<std::size_t>(maxTracks, 1, 5000);
    std::vector<PresenceSnapshot> snapshot;
    snapshot.reserve(maxTracks + 1);
    {
        PresenceStatement statement(
            db, "SELECT id,path,missing FROM tracks WHERE id>?1 ORDER BY id LIMIT ?2;");
        if (!statement.ready()
            || sqlite3_bind_int64(statement.get(), 1, afterTrackId) != SQLITE_OK
            || sqlite3_bind_int64(statement.get(), 2,
                                  static_cast<sqlite3_int64>(maxTracks + 1)) != SQLITE_OK) {
            setPresenceError(error, db, "prepare file-presence snapshot");
            return std::nullopt;
        }

        for (;;) {
            const int step = sqlite3_step(statement.get());
            if (step == SQLITE_DONE) break;
            if (step != SQLITE_ROW) {
                setPresenceError(error, db, "read file-presence snapshot");
                return std::nullopt;
            }
            snapshot.push_back(PresenceSnapshot{
                sqlite3_column_int64(statement.get(), 0),
                presenceColumnText(statement.get(), 1),
                sqlite3_column_int(statement.get(), 2) != 0});
        }
    }

    FilePresenceRefreshResult result;
    if (snapshot.size() > maxTracks) {
        result.complete = false;
        snapshot.resize(maxTracks);
    }
    result.scanned = snapshot.size();
    if (!snapshot.empty()) result.nextAfterTrackId = snapshot.back().id;

    std::vector<PresenceDecision> decisions;
    decisions.reserve(snapshot.size());
    for (const auto& item : snapshot) {
        const auto path = presencePathFromUtf8(item.path);
        std::error_code filesystemError;
        const bool exists = std::filesystem::exists(path, filesystemError);
        if (filesystemError) {
            ++result.unresolved;
            continue;
        }

        bool missingNow = !exists;
        if (exists) {
            const bool regularFile = std::filesystem::is_regular_file(path, filesystemError);
            if (filesystemError) {
                ++result.unresolved;
                continue;
            }
            missingNow = !regularFile;
        }

        if (missingNow) ++result.missing;
        if (missingNow != item.missing)
            decisions.push_back(PresenceDecision{item, missingNow});
    }

    if (decisions.empty()) return result;
    if (!presenceExec(db, "BEGIN IMMEDIATE;", error)) return std::nullopt;

    PresenceStatement update(
        db, "UPDATE tracks SET missing=?1 WHERE id=?2 AND path=?3 AND missing=?4;");
    if (!update.ready()) {
        setPresenceError(error, db, "prepare file-presence update");
        static_cast<void>(presenceExec(db, "ROLLBACK;", nullptr));
        return std::nullopt;
    }

    for (const auto& decision : decisions) {
        sqlite3_reset(update.get());
        sqlite3_clear_bindings(update.get());
        const bool bound = sqlite3_bind_int(update.get(), 1, decision.missingNow ? 1 : 0) == SQLITE_OK
            && sqlite3_bind_int64(update.get(), 2, decision.snapshot.id) == SQLITE_OK
            && bindPresenceText(update.get(), 3, decision.snapshot.path)
            && sqlite3_bind_int(update.get(), 4, decision.snapshot.missing ? 1 : 0) == SQLITE_OK;
        if (!bound || sqlite3_step(update.get()) != SQLITE_DONE) {
            setPresenceError(error, db, "update file-presence flag");
            static_cast<void>(presenceExec(db, "ROLLBACK;", nullptr));
            return std::nullopt;
        }
        if (sqlite3_changes(db) == 1) ++result.changed;
    }

    if (!presenceExec(db, "COMMIT;", error)) {
        static_cast<void>(presenceExec(db, "ROLLBACK;", nullptr));
        return std::nullopt;
    }
    return result;
}

} // namespace broke::library
