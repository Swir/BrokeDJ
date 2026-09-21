// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "FileContentHash.h"
#include "LibraryDatabase.h"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace broke::library::runtime {

struct PlayedTrackRow final {
    TrackRecord track;
    std::int64_t historyId = -1;
    std::int64_t playedAtUnixMs = 0;
};

struct ContentHashBackfillResult final {
    std::size_t scanned = 0;
    std::size_t hashed = 0;
    std::size_t markedMissing = 0;
    std::size_t skipped = 0;
    std::size_t failed = 0;
    bool cancelled = false;
};

namespace detail {

class Statement final {
public:
    Statement(sqlite3* database, const char* sql) {
        if (database != nullptr)
            result = sqlite3_prepare_v2(database, sql, -1, &statement, nullptr);
    }
    ~Statement() {
        if (statement != nullptr) sqlite3_finalize(statement);
    }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    [[nodiscard]] bool ready() const noexcept {
        return result == SQLITE_OK && statement != nullptr;
    }
    [[nodiscard]] sqlite3_stmt* get() const noexcept { return statement; }

private:
    sqlite3_stmt* statement = nullptr;
    int result = SQLITE_MISUSE;
};

inline void setError(std::string* output, sqlite3* database, std::string_view prefix) {
    if (output == nullptr || !output->empty()) return;
    output->assign(prefix);
    if (database != nullptr) {
        const char* message = sqlite3_errmsg(database);
        if (message != nullptr && *message != '\0') {
            output->append(": ");
            output->append(message);
        }
    }
}

inline void setError(std::string* output, std::string_view message) {
    if (output != nullptr && output->empty()) output->assign(message);
}

[[nodiscard]] inline std::string pathUtf8(const std::filesystem::path& path) {
    const auto value = path.lexically_normal().generic_u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

[[nodiscard]] inline std::filesystem::path pathFromUtf8(std::string_view value) {
    std::u8string utf8;
    utf8.reserve(value.size());
    for (const auto ch : value)
        utf8.push_back(static_cast<char8_t>(static_cast<unsigned char>(ch)));
    return std::filesystem::path(utf8);
}

[[nodiscard]] inline std::string columnText(sqlite3_stmt* statement, int column) {
    const auto* value = sqlite3_column_text(statement, column);
    if (value == nullptr) return {};
    const int bytes = sqlite3_column_bytes(statement, column);
    return std::string(reinterpret_cast<const char*>(value), static_cast<std::size_t>(bytes));
}

[[nodiscard]] inline TrackRecord readTrack(sqlite3_stmt* statement, int offset = 0) {
    TrackRecord track;
    track.id = sqlite3_column_int64(statement, offset + 0);
    track.path = columnText(statement, offset + 1);
    track.fileSize = sqlite3_column_int64(statement, offset + 2);
    track.modifiedNs = sqlite3_column_int64(statement, offset + 3);
    track.title = columnText(statement, offset + 4);
    track.artist = columnText(statement, offset + 5);
    track.album = columnText(statement, offset + 6);
    track.durationSeconds = sqlite3_column_double(statement, offset + 7);
    if (sqlite3_column_type(statement, offset + 8) != SQLITE_NULL)
        track.bpm = sqlite3_column_double(statement, offset + 8);
    track.musicalKey = columnText(statement, offset + 9);
    track.contentHash = columnText(statement, offset + 10);
    track.missing = sqlite3_column_int(statement, offset + 11) != 0;
    return track;
}

[[nodiscard]] inline sqlite3* openDatabase(const std::filesystem::path& databaseFile,
                                           int flags,
                                           std::string* error) {
    if (error != nullptr) error->clear();
    if (databaseFile.empty()) {
        setError(error, "Library database path is empty");
        return nullptr;
    }
    sqlite3* database = nullptr;
    const auto path = pathUtf8(databaseFile);
    const int result = sqlite3_open_v2(path.c_str(), &database, flags | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (result != SQLITE_OK || database == nullptr) {
        setError(error, database, "open library runtime database");
        if (database != nullptr) sqlite3_close_v2(database);
        return nullptr;
    }
    if (sqlite3_busy_timeout(database, 2500) != SQLITE_OK) {
        setError(error, database, "configure library runtime busy timeout");
        sqlite3_close_v2(database);
        return nullptr;
    }
    return database;
}

[[nodiscard]] inline std::optional<std::string> sha256FileCancellable(
    const std::filesystem::path& path,
    const std::atomic<bool>* cancelled) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::nullopt;

    broke::library::detail::Sha256 hash;
    std::array<char, 64 * 1024> buffer{};
    while (input) {
        if (cancelled != nullptr && cancelled->load(std::memory_order_acquire))
            return std::nullopt;
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto read = input.gcount();
        if (read > 0)
            hash.update(reinterpret_cast<const std::byte*>(buffer.data()),
                        static_cast<std::size_t>(read));
    }
    if (input.bad()) return std::nullopt;
    if (cancelled != nullptr && cancelled->load(std::memory_order_acquire))
        return std::nullopt;
    return hash.finishHex();
}

[[nodiscard]] inline bool filesystemStatusMeansMissing(const std::error_code& error) noexcept {
    return !error
        || error == std::errc::no_such_file_or_directory
        || error == std::errc::not_a_directory;
}

constexpr const char* trackColumns =
    "t.id,t.path,t.file_size,t.modified_ns,t.title,t.artist,t.album,"
    "t.duration_seconds,t.bpm,t.musical_key,t.content_hash,t.missing";

} // namespace detail

[[nodiscard]] inline std::optional<TrackRecord> findTrackByPath(
    const std::filesystem::path& databaseFile,
    std::string_view path,
    std::string* error = nullptr) {
    sqlite3* database = detail::openDatabase(databaseFile, SQLITE_OPEN_READONLY, error);
    if (database == nullptr) return std::nullopt;

    const std::string sql = std::string("SELECT ") + detail::trackColumns
        + " FROM tracks t WHERE t.path=?1 LIMIT 1;";
    detail::Statement statement(database, sql.c_str());
    std::optional<TrackRecord> result;
    if (!statement.ready()
        || sqlite3_bind_text(statement.get(), 1, path.data(), static_cast<int>(path.size()),
                             SQLITE_TRANSIENT) != SQLITE_OK) {
        detail::setError(error, database, "prepare exact library path lookup");
    } else {
        const int step = sqlite3_step(statement.get());
        if (step == SQLITE_ROW)
            result = detail::readTrack(statement.get());
        else if (step != SQLITE_DONE)
            detail::setError(error, database, "read exact library path lookup");
    }
    sqlite3_close_v2(database);
    return result;
}

[[nodiscard]] inline std::vector<PlayedTrackRow> recentPlayedTracks(
    const std::filesystem::path& databaseFile,
    std::size_t limit = 200,
    std::string* error = nullptr) {
    std::vector<PlayedTrackRow> rows;
    sqlite3* database = detail::openDatabase(databaseFile, SQLITE_OPEN_READONLY, error);
    if (database == nullptr) return rows;

    limit = std::clamp<std::size_t>(limit, 1, 1000);
    const std::string sql = std::string("SELECT ") + detail::trackColumns
        + ",h.id,h.played_at_ms FROM history h JOIN tracks t ON t.id=h.track_id "
          "ORDER BY h.played_at_ms DESC,h.id DESC LIMIT ?1;";
    detail::Statement statement(database, sql.c_str());
    if (!statement.ready()
        || sqlite3_bind_int64(statement.get(), 1, static_cast<sqlite3_int64>(limit)) != SQLITE_OK) {
        detail::setError(error, database, "prepare played-track history query");
        sqlite3_close_v2(database);
        return rows;
    }

    for (;;) {
        const int step = sqlite3_step(statement.get());
        if (step == SQLITE_DONE) break;
        if (step != SQLITE_ROW) {
            detail::setError(error, database, "read played-track history");
            rows.clear();
            break;
        }
        PlayedTrackRow row;
        row.track = detail::readTrack(statement.get());
        row.historyId = sqlite3_column_int64(statement.get(), 12);
        row.playedAtUnixMs = sqlite3_column_int64(statement.get(), 13);
        rows.push_back(std::move(row));
    }

    sqlite3_close_v2(database);
    return rows;
}

[[nodiscard]] inline ContentHashBackfillResult backfillContentHashes(
    const std::filesystem::path& databaseFile,
    std::size_t limit = 64,
    const std::atomic<bool>* cancelled = nullptr,
    std::string* error = nullptr) {
    ContentHashBackfillResult result;
    sqlite3* database = detail::openDatabase(databaseFile, SQLITE_OPEN_READWRITE, error);
    if (database == nullptr) return result;
    limit = std::clamp<std::size_t>(limit, 1, 256);

    struct Candidate final {
        std::int64_t id = -1;
        std::string path;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(limit);
    {
        detail::Statement select(database,
            "SELECT id,path FROM tracks WHERE missing=0 AND content_hash='' ORDER BY id LIMIT ?1;");
        if (!select.ready()
            || sqlite3_bind_int64(select.get(), 1, static_cast<sqlite3_int64>(limit)) != SQLITE_OK) {
            detail::setError(error, database, "prepare content-hash backfill scan");
            sqlite3_close_v2(database);
            return result;
        }
        for (;;) {
            const int step = sqlite3_step(select.get());
            if (step == SQLITE_DONE) break;
            if (step != SQLITE_ROW) {
                detail::setError(error, database, "scan content-hash backfill candidates");
                sqlite3_close_v2(database);
                return result;
            }
            candidates.push_back(Candidate{sqlite3_column_int64(select.get(), 0),
                                           detail::columnText(select.get(), 1)});
        }
    }

    for (const auto& candidate : candidates) {
        if (cancelled != nullptr && cancelled->load(std::memory_order_acquire)) {
            result.cancelled = true;
            break;
        }
        ++result.scanned;
        const auto file = detail::pathFromUtf8(candidate.path);
        std::error_code filesystemError;
        const bool regularFile = std::filesystem::is_regular_file(file, filesystemError);
        if (!regularFile) {
            if (!detail::filesystemStatusMeansMissing(filesystemError)) {
                ++result.failed;
                continue;
            }
            detail::Statement markMissing(database,
                "UPDATE tracks SET missing=1 WHERE id=?1 AND path=?2 AND content_hash='' AND missing=0;");
            if (!markMissing.ready()
                || sqlite3_bind_int64(markMissing.get(), 1, candidate.id) != SQLITE_OK
                || sqlite3_bind_text(markMissing.get(), 2, candidate.path.data(),
                                     static_cast<int>(candidate.path.size()), SQLITE_TRANSIENT) != SQLITE_OK
                || sqlite3_step(markMissing.get()) != SQLITE_DONE) {
                ++result.failed;
                detail::setError(error, database, "mark missing legacy hash candidate");
            } else if (sqlite3_changes(database) == 1) {
                ++result.markedMissing;
            } else {
                ++result.skipped;
            }
            continue;
        }

        const auto sizeBefore = std::filesystem::file_size(file, filesystemError);
        if (filesystemError) {
            ++result.failed;
            continue;
        }
        const auto modifiedBefore = std::filesystem::last_write_time(file, filesystemError);
        if (filesystemError) {
            ++result.failed;
            continue;
        }
        const auto hash = detail::sha256FileCancellable(file, cancelled);
        if (!hash) {
            if (cancelled != nullptr && cancelled->load(std::memory_order_acquire)) {
                result.cancelled = true;
                break;
            }
            ++result.failed;
            continue;
        }
        const auto sizeAfter = std::filesystem::file_size(file, filesystemError);
        if (filesystemError) {
            ++result.failed;
            continue;
        }
        const auto modifiedAfter = std::filesystem::last_write_time(file, filesystemError);
        if (filesystemError) {
            ++result.failed;
            continue;
        }
        if (sizeBefore != sizeAfter || modifiedBefore != modifiedAfter) {
            ++result.skipped;
            continue;
        }

        detail::Statement update(database,
            "UPDATE tracks SET content_hash=?1,file_size=?2,missing=0 "
            "WHERE id=?3 AND path=?4 AND content_hash='' AND missing=0;");
        if (!update.ready()
            || sqlite3_bind_text(update.get(), 1, hash->data(), static_cast<int>(hash->size()),
                                 SQLITE_TRANSIENT) != SQLITE_OK
            || sqlite3_bind_int64(update.get(), 2, static_cast<sqlite3_int64>(sizeAfter)) != SQLITE_OK
            || sqlite3_bind_int64(update.get(), 3, candidate.id) != SQLITE_OK
            || sqlite3_bind_text(update.get(), 4, candidate.path.data(),
                                 static_cast<int>(candidate.path.size()), SQLITE_TRANSIENT) != SQLITE_OK
            || sqlite3_step(update.get()) != SQLITE_DONE) {
            ++result.failed;
            detail::setError(error, database, "publish legacy content hash");
        } else if (sqlite3_changes(database) == 1) {
            ++result.hashed;
        } else {
            ++result.skipped;
        }
    }

    sqlite3_close_v2(database);
    return result;
}

} // namespace broke::library::runtime
