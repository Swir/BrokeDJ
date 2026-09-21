// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#include "LibraryDatabase.h"
#include "FileContentHash.h"

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
        if (db != nullptr) result = sqlite3_prepare_v2(db, sql, -1, &statement, nullptr);
    }
    ~Statement() { if (statement != nullptr) sqlite3_finalize(statement); }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    [[nodiscard]] bool ready() const noexcept { return result == SQLITE_OK && statement != nullptr; }
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

[[nodiscard]] std::filesystem::path pathFromUtf8(std::string_view value) {
    std::u8string utf8;
    utf8.reserve(value.size());
    for (const auto ch : value)
        utf8.push_back(static_cast<char8_t>(static_cast<unsigned char>(ch)));
    return std::filesystem::path(utf8);
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
    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
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

[[nodiscard]] std::optional<std::string> populateHashWhenLocalFileExists(
    const TrackRecord& track, std::string* error) {
    if (!track.contentHash.empty()) return track.contentHash;
    const auto path = pathFromUtf8(track.path);
