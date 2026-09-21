// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;

namespace broke::library {

struct TrackRecord final {
    std::int64_t id = -1;
    std::string path;
    std::int64_t fileSize = 0;
    std::int64_t modifiedNs = 0;
    std::string contentHash;
    std::string title;
    std::string artist;
    std::string album;
    double durationSeconds = 0.0;
    std::optional<double> bpm;
    std::string musicalKey;
    bool missing = false;
};

struct HistoryEntry final {
    std::int64_t id = -1;
    std::int64_t trackId = -1;
    std::int64_t playedAtUnixMs = 0;
};

struct DuplicateGroup final {
    std::string contentHash;
    std::vector<TrackRecord> tracks;
};

class LibraryDatabase final {
public:
    static constexpr int currentSchemaVersion = 2;

    LibraryDatabase() = default;
    ~LibraryDatabase();

    LibraryDatabase(const LibraryDatabase&) = delete;
    LibraryDatabase& operator=(const LibraryDatabase&) = delete;
    LibraryDatabase(LibraryDatabase&& other) noexcept;
    LibraryDatabase& operator=(LibraryDatabase&& other) noexcept;

    [[nodiscard]] bool open(const std::filesystem::path& file, std::string* error = nullptr);
    void close() noexcept;
    [[nodiscard]] bool isOpen() const noexcept { return db != nullptr; }
    [[nodiscard]] int schemaVersion() const noexcept;

    [[nodiscard]] std::optional<std::int64_t> upsertTrack(
        const TrackRecord& track, std::string* error = nullptr);
    [[nodiscard]] bool markMissing(std::int64_t trackId, bool missing,
                                   std::string* error = nullptr);
    [[nodiscard]] bool relocateTrack(std::int64_t trackId, const std::filesystem::path& newPath,
                                     std::int64_t fileSize, std::int64_t modifiedNs,
                                     std::string* error = nullptr);

    [[nodiscard]] std::vector<TrackRecord> search(
        std::string_view query, std::size_t limit = 100, std::string* error = nullptr) const;

    [[nodiscard]] bool addTag(std::int64_t trackId, std::string_view tag,
                              std::string* error = nullptr);
    [[nodiscard]] bool removeTag(std::int64_t trackId, std::string_view tag,
                                 std::string* error = nullptr);
    [[nodiscard]] std::vector<std::string> tagsForTrack(
        std::int64_t trackId, std::string* error = nullptr) const;

    [[nodiscard]] std::optional<std::int64_t> createPlaylist(
        std::string_view name, std::string* error = nullptr);
    [[nodiscard]] std::optional<std::int64_t> findPlaylist(
        std::string_view name, std::string* error = nullptr) const;
    [[nodiscard]] bool addToPlaylist(std::int64_t playlistId, std::int64_t trackId,
                                     std::string* error = nullptr);
    [[nodiscard]] bool removeFromPlaylist(std::int64_t playlistId, std::int64_t trackId,
                                          std::string* error = nullptr);
    [[nodiscard]] std::vector<TrackRecord> playlistTracks(
        std::int64_t playlistId, std::string* error = nullptr) const;

    [[nodiscard]] bool recordPlay(std::int64_t trackId, std::int64_t playedAtUnixMs,
                                  std::string* error = nullptr);
    [[nodiscard]] std::vector<HistoryEntry> recentHistory(
        std::size_t limit = 100, std::string* error = nullptr) const;

    [[nodiscard]] std::vector<DuplicateGroup> duplicateGroups(
        std::size_t maxTracks = 256, std::string* error = nullptr) const;

    [[nodiscard]] bool backupTo(const std::filesystem::path& destination,
                                std::string* error = nullptr) const;
    [[nodiscard]] bool restoreFrom(const std::filesystem::path& source,
                                   std::string* error = nullptr);
    [[nodiscard]] bool integrityCheck(std::string* error = nullptr) const;

private:
    [[nodiscard]] bool configure(std::string* error);
    [[nodiscard]] bool migrate(std::string* error);
    [[nodiscard]] bool exec(const char* sql, std::string* error) const;

    sqlite3* db = nullptr;
};

} // namespace broke::library
