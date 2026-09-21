// SPDX-License-Identifier: AGPL-3.0-only
#include "app/LibraryDatabase.h"
#include "app/SessionStore.h"

#include <sqlite3.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct TempDirectory final {
    std::filesystem::path path;
    TempDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path()
            / ("brokedj-library-tests-" + std::to_string(stamp));
        std::filesystem::create_directories(path);
    }
    ~TempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

broke::library::TrackRecord track(std::string path, std::string title,
                                  std::string artist, std::string hash) {
    broke::library::TrackRecord value;
    value.path = std::move(path);
    value.fileSize = 123456;
    value.modifiedNs = 987654321;
    value.contentHash = std::move(hash);
    value.title = std::move(title);
    value.artist = std::move(artist);
    value.album = "Fixture Album";
    value.durationSeconds = 180.0;
    value.bpm = 140.0;
    value.musicalKey = "8A";
    return value;
}

void writeBytes(const std::filesystem::path& path, std::string_view bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    check(static_cast<bool>(output), "open fixture");
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(output), "write fixture");
}

void execFixture(sqlite3* db, const char* sql, const char* message) {
    char* error = nullptr;
    const int result = sqlite3_exec(db, sql, nullptr, nullptr, &error);
    if (result != SQLITE_OK) {
        const std::string detail = error != nullptr ? error : "unknown sqlite error";
        sqlite3_free(error);
        throw std::runtime_error(std::string(message) + ": " + detail);
    }
}

void createVersionOneDatabase(const std::filesystem::path& path) {
    sqlite3* db = nullptr;
    check(sqlite3_open_v2(path.string().c_str(), &db,
                          SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK,
          "create v1 fixture");
    execFixture(db,
        "PRAGMA foreign_keys=ON;"
        "CREATE TABLE tracks(id INTEGER PRIMARY KEY,path TEXT NOT NULL UNIQUE,file_size INTEGER NOT NULL CHECK(file_size>=0),modified_ns INTEGER NOT NULL,title TEXT NOT NULL DEFAULT '',artist TEXT NOT NULL DEFAULT '',album TEXT NOT NULL DEFAULT '',duration_seconds REAL NOT NULL DEFAULT 0 CHECK(duration_seconds>=0),bpm REAL,musical_key TEXT NOT NULL DEFAULT '',added_at_ms INTEGER NOT NULL,last_seen_at_ms INTEGER NOT NULL);"
        "CREATE TABLE tags(id INTEGER PRIMARY KEY,name TEXT NOT NULL COLLATE NOCASE UNIQUE CHECK(length(trim(name))>0));"
        "CREATE TABLE track_tags(track_id INTEGER NOT NULL REFERENCES tracks(id) ON DELETE CASCADE,tag_id INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,PRIMARY KEY(track_id,tag_id));"
        "CREATE TABLE playlists(id INTEGER PRIMARY KEY,name TEXT NOT NULL COLLATE NOCASE UNIQUE CHECK(length(trim(name))>0),created_at_ms INTEGER NOT NULL);"
        "CREATE TABLE playlist_items(playlist_id INTEGER NOT NULL REFERENCES playlists(id) ON DELETE CASCADE,track_id INTEGER NOT NULL REFERENCES tracks(id) ON DELETE CASCADE,position INTEGER NOT NULL CHECK(position>=0),PRIMARY KEY(playlist_id,track_id),UNIQUE(playlist_id,position));"
        "CREATE TABLE history(id INTEGER PRIMARY KEY,track_id INTEGER NOT NULL REFERENCES tracks(id) ON DELETE CASCADE,played_at_ms INTEGER NOT NULL);"
        "INSERT INTO tracks(path,file_size,modified_ns,title,artist,album,duration_seconds,bpm,musical_key,added_at_ms,last_seen_at_ms) VALUES('legacy.wav',42,11,'Legacy','Fixture','','60',128,'7A',1,1);"
        "PRAGMA user_version=1;", "create v1 schema");
    check(sqlite3_close_v2(db) == SQLITE_OK, "close v1 fixture");
}

void createFutureDatabase(const std::filesystem::path& path) {
    sqlite3* db = nullptr;
    check(sqlite3_open_v2(path.string().c_str(), &db,
                          SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK,
          "create future fixture");
    execFixture(db, "PRAGMA user_version=999;", "set future version");
    check(sqlite3_close_v2(db) == SQLITE_OK, "close future fixture");
}

void createMalformedCurrentDatabase(const std::filesystem::path& path) {
    sqlite3* db = nullptr;
    check(sqlite3_open_v2(path.string().c_str(), &db,
                          SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK,
          "create malformed-current fixture");
    execFixture(db,
        "CREATE TABLE unrelated(id INTEGER PRIMARY KEY);"
        "PRAGMA user_version=2;",
        "create malformed-current schema");
    check(sqlite3_close_v2(db) == SQLITE_OK, "close malformed-current fixture");
}

void createVersionZeroGenericDatabase(const std::filesystem::path& path) {
    sqlite3* db = nullptr;
    check(sqlite3_open_v2(path.string().c_str(), &db,
                          SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK,
          "create version-zero generic fixture");
    execFixture(db,
        "CREATE TABLE unrelated(id INTEGER PRIMARY KEY,payload TEXT NOT NULL);"
        "INSERT INTO unrelated(payload) VALUES('not-a-brokedj-backup');"
        "PRAGMA user_version=0;",
        "create version-zero generic schema");
    check(sqlite3_close_v2(db) == SQLITE_OK, "close version-zero generic fixture");
}

void testLibraryWorkflowAndSafeRestore() {
    TempDirectory temp;
    std::string error;
    broke::library::LibraryDatabase db;
    check(db.open(temp.path / "library.sqlite3", &error), "open fresh library");
    check(db.schemaVersion() == broke::library::LibraryDatabase::currentSchemaVersion,
          "fresh library migrated");
    check(db.integrityCheck(&error), "fresh library integrity");

    auto alpha = track("music/Alpha.wav", "Alpha", "Alice", "sha256-A");
    auto beta = track("music/Beta.wav", "Beta", "Bob", "sha256-B");
    auto alphaCopy = track("music/Alpha Copy.wav", "Alpha Copy", "Alice", "sha256-A");
    const auto alphaId = db.upsertTrack(alpha, &error);
    const auto betaId = db.upsertTrack(beta, &error);
    const auto alphaCopyId = db.upsertTrack(alphaCopy, &error);
    check(alphaId && betaId && alphaCopyId, "insert tracks");

    alpha.album = "Updated Album";
    const auto alphaAgain = db.upsertTrack(alpha, &error);
    check(alphaAgain && *alphaAgain == *alphaId, "upsert stable id");

    check(db.addTag(*alphaId, "Techno", &error), "add tag");
    check(db.addTag(*alphaId, "Peak Time", &error), "add second tag");
    check(db.addTag(*alphaId, "TECHNO", &error), "tag idempotent");
    check(db.tagsForTrack(*alphaId, &error).size() == 2, "unique tags");
    check(db.search("peak", 20, &error).size() == 1, "tag search");
    check(db.search("alice", 20, &error).size() == 2, "artist search");

    const auto playlist = db.createPlaylist("Tonight", &error);
    check(playlist.has_value(), "create playlist");
    check(db.addToPlaylist(*playlist, *alphaId, &error), "playlist alpha");
    check(db.addToPlaylist(*playlist, *betaId, &error), "playlist beta");
    check(db.addToPlaylist(*playlist, *alphaId, &error), "playlist idempotent");
    auto playlistTracks = db.playlistTracks(*playlist, &error);
    check(playlistTracks.size() == 2 && playlistTracks[0].id == *alphaId
              && playlistTracks[1].id == *betaId,
          "playlist insertion order");
    check(db.removeFromPlaylist(*playlist, *betaId, &error), "playlist remove");
    playlistTracks = db.playlistTracks(*playlist, &error);
    check(playlistTracks.size() == 1 && playlistTracks.front().id == *alphaId,
          "playlist remove preserves remaining item");

    check(db.recordPlay(*alphaId, 1000, &error), "history alpha");
    check(db.recordPlay(*betaId, 2000, &error), "history beta");
    const auto history = db.recentHistory(10, &error);
    check(history.size() == 2 && history.front().trackId == *betaId
              && history.back().trackId == *alphaId,
          "history newest first");

    const auto duplicateGroups = db.duplicateGroups(32, &error);
    check(duplicateGroups.size() == 1 && duplicateGroups.front().contentHash == "sha256-A"
              && duplicateGroups.front().tracks.size() == 2,
          "duplicate group");
    check(db.search(broke::library::LibraryDatabase::duplicateSearchDirective, 32, &error).size() == 2,
          "duplicate search directive");
    check(db.markMissing(*alphaCopyId, true, &error), "mark missing");
    check(db.duplicateGroups(32, &error).empty(), "missing excluded from duplicates");
    const auto missing = db.search(broke::library::LibraryDatabase::missingSearchDirective, 32, &error);
    check(missing.size() == 1 && missing.front().id == *alphaCopyId,
          "missing search directive");
    check(db.markMissing(*alphaCopyId, false, &error), "clear missing");

    const auto relocatedPath = temp.path / "moved" / "Alpha.wav";
    writeBytes(relocatedPath, "relocated-alpha");
    check(db.markMissing(*alphaId, true, &error), "mark relocated source missing");
    check(db.relocateTrack(*alphaId, relocatedPath,
                           static_cast<std::int64_t>(std::filesystem::file_size(relocatedPath)),
                           123456789, &error), "relocate source");
    const auto relocated = db.search("Alpha.wav", 20, &error);
    bool foundRelocated = false;
    for (const auto& item : relocated)
        if (item.id == *alphaId)
            foundRelocated = !item.missing && item.contentHash.size() == 64
                && item.path.find("moved/Alpha.wav") != std::string::npos;
    check(foundRelocated, "relocation refreshes identity");
    check(db.tagsForTrack(*alphaId, &error).size() == 2, "relocation preserves tags");
    check(db.playlistTracks(*playlist, &error).front().id == *alphaId,
          "relocation preserves playlist membership");

    const auto backup = temp.path / "backup.sqlite3";
    check(db.backupTo(backup, &error), "verified backup");
    broke::library::LibraryDatabase backupProbe;
    check(backupProbe.open(backup, &error), "verified backup reopens");
    check(backupProbe.integrityCheck(&error), "verified backup structural integrity");
    backupProbe.close();

    auto afterBackup = track("music/After Backup.wav", "After Backup", "Carol", "sha256-C");
    check(db.upsertTrack(afterBackup, &error).has_value(), "mutate after backup");
    check(db.search("", 100, &error).size() == 4, "post-backup mutation visible");
    check(db.restoreFrom(backup, &error), "restore verified backup");
    check(db.search("", 100, &error).size() == 3, "restore returns backup snapshot");
    check(db.integrityCheck(&error), "restored database integrity");

    const auto corrupt = temp.path / "corrupt.sqlite3";
    writeBytes(corrupt, "not a sqlite database");
    check(!db.restoreFrom(corrupt, &error), "corrupt restore rejected");
    check(db.search("", 100, nullptr).size() == 3, "corrupt restore preserves live snapshot");

    const auto malformed = temp.path / "malformed-current.sqlite3";
    createMalformedCurrentDatabase(malformed);
    check(!db.restoreFrom(malformed, &error),
          "quick-check-valid but structurally invalid current-schema backup rejected");
    check(db.search("", 100, nullptr).size() == 3,
          "malformed-current restore preserves live snapshot");
    check(db.integrityCheck(&error), "live snapshot remains structurally valid after rejection");

    const auto genericV0 = temp.path / "generic-v0.sqlite3";
    createVersionZeroGenericDatabase(genericV0);
    check(!db.restoreFrom(genericV0, &error),
          "generic version-zero SQLite database rejected as unsupported backup");
    check(error.find("unsupported") != std::string::npos,
          "version-zero rejection identifies unsupported backup schema");
    check(db.search("", 100, nullptr).size() == 3,
          "version-zero rejection preserves live snapshot");
    check(db.integrityCheck(&error),
          "live snapshot remains valid after version-zero rejection");

    const auto future = temp.path / "future.sqlite3";
    createFutureDatabase(future);
    check(!db.restoreFrom(future, &error), "future backup rejected");
    check(db.search("", 100, nullptr).size() == 3, "future rejection preserves live snapshot");

    const auto v1Restore = temp.path / "v1-restore.sqlite3";
    createVersionOneDatabase(v1Restore);
    check(db.restoreFrom(v1Restore, &error), "v1 backup stages and migrates before install");
    check(db.schemaVersion() == broke::library::LibraryDatabase::currentSchemaVersion,
          "restored v1 backup migrated to current schema");
    check(db.search("Legacy", 10, &error).size() == 1, "restored v1 data preserved");
    check(db.integrityCheck(&error), "restored v1 snapshot structurally valid");
}

void testAutomaticContentHashing() {
    TempDirectory temp;
    std::string error;
    broke::library::LibraryDatabase db;
    check(db.open(temp.path / "hash-library.sqlite3", &error), "open hash library");
    const auto first = temp.path / "music" / "first.wav";
    const auto second = temp.path / "music" / "second.wav";
    writeBytes(first, "abc");
    writeBytes(second, "abc");
    auto firstTrack = track(first.generic_string(), "First", "Fixture", ""); firstTrack.fileSize = 3;
    auto secondTrack = track(second.generic_string(), "Second", "Fixture", ""); secondTrack.fileSize = 3;
    const auto firstId = db.upsertTrack(firstTrack, &error);
    const auto secondId = db.upsertTrack(secondTrack, &error);
    check(firstId && secondId, "hash local imports");
    const auto rows = db.search("Fixture", 10, &error);
    constexpr std::string_view abcSha256 =
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    check(rows.size() == 2 && rows[0].contentHash == abcSha256 && rows[1].contentHash == abcSha256,
          "SHA-256 reference vector");
    check(db.search(broke::library::LibraryDatabase::duplicateSearchDirective, 10, &error).size() == 2,
          "automatic hashes feed duplicate review");
    const auto moved = temp.path / "moved" / "first.wav";
    writeBytes(moved, "different-content");
    check(db.relocateTrack(*firstId, moved,
                           static_cast<std::int64_t>(std::filesystem::file_size(moved)), 222, &error),
          "relocation rehashes");
    check(db.search(broke::library::LibraryDatabase::duplicateSearchDirective, 10, &error).empty(),
          "rehash removes stale duplicate identity");
    const auto movedResult = db.search("moved", 10, &error);
    check(movedResult.size() == 1 && movedResult.front().contentHash.size() == 64
              && movedResult.front().contentHash != abcSha256,
          "relocated track stores refreshed SHA-256");
}

void testSyntheticLargeLibraryQueries() {
    TempDirectory temp;
    std::string error;
    broke::library::LibraryDatabase db;
    check(db.open(temp.path / "large.sqlite3", &error), "open synthetic library");
    constexpr int trackCount = 1500;
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < trackCount; ++i) {
        auto value = track("virtual/track-" + std::to_string(i) + ".wav",
                           i == trackCount - 1 ? "Needle Track 1499" : "Track " + std::to_string(i),
                           "Stress Artist",
                           i % 300 == 0 ? "stress-duplicate" : "hash-" + std::to_string(i));
        check(db.upsertTrack(value, &error).has_value(), "populate synthetic library");
    }
    check(db.search("", 100, &error).size() == 100, "search bound");
    check(db.search("Needle Track 1499", 10, &error).size() == 1, "targeted search");
    check(db.search(broke::library::LibraryDatabase::duplicateSearchDirective, 100, &error).size() == 5,
          "bounded duplicate query");
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    std::cout << "Synthetic library diagnostic: " << trackCount
              << " rows inserted and queried in " << elapsed << " ms\n";
}

void testMigrationAndFutureSchema() {
    TempDirectory temp;
    const auto legacy = temp.path / "legacy.sqlite3";
    createVersionOneDatabase(legacy);
    std::string error;
    broke::library::LibraryDatabase db;
    check(db.open(legacy, &error), "open legacy database");
    check(db.schemaVersion() == broke::library::LibraryDatabase::currentSchemaVersion,
          "legacy migrated");
    const auto tracks = db.search("Legacy", 10, &error);
    check(tracks.size() == 1 && tracks.front().contentHash.empty() && !tracks.front().missing,
          "migration preserves data and v2 defaults");
    check(db.integrityCheck(&error), "migrated integrity");

    const auto malformed = temp.path / "malformed.sqlite3";
    createMalformedCurrentDatabase(malformed);
    broke::library::LibraryDatabase malformedDb;
    check(!malformedDb.open(malformed, &error), "malformed current schema fails closed on open");
    check(!malformedDb.isOpen(), "malformed open leaves handle closed");

    const auto future = temp.path / "future.sqlite3";
    createFutureDatabase(future);
    broke::library::LibraryDatabase futureDb;
    check(!futureDb.open(future, &error), "future schema fails closed");
    check(!futureDb.isOpen(), "future schema handle closed");
}

void testSessionPersistence() {
    TempDirectory temp;
    const auto sessionPath = temp.path / "sets" / "last-session.bds";
    broke::session::SessionStore store;
    broke::session::SessionState state;
    state.mixer.crossfader = 0.25f;
    state.mixer.master = 0.75f;
    state.mixer.headphoneLevel = 0.4f;
    state.decks[0].path = "music/Alpha.wav";
    state.decks[0].positionSeconds = 42.5;
    state.decks[0].playbackRate = 1.05f;
    state.decks[0].trimDb = -2.0f;
    state.decks[0].channelGain = 0.8f;
    state.decks[0].low = 0.9f;
    state.decks[0].mid = 1.1f;
    state.decks[0].high = 1.2f;
    state.decks[0].echo = 0.2f;
    state.decks[0].drive = 1.5f;
    state.decks[0].headphoneCue = true;
    state.decks[0].wholeTrackLoop = true;
    state.decks[0].wasPlaying = true;
    state.decks[3].path = "music/Deck D.flac";
    state.decks[3].positionSeconds = 301.0;

    std::string error;
    check(store.save(sessionPath, state, &error), "save session");
    const auto loaded = store.load(sessionPath, &error);
    check(loaded.has_value(), "load session snapshot");
    check(loaded->mixer.crossfader == state.mixer.crossfader
              && loaded->mixer.master == state.mixer.master
              && loaded->mixer.headphoneLevel == state.mixer.headphoneLevel,
          "restore mixer session state");
    check(loaded->decks[0].path == state.decks[0].path
              && loaded->decks[0].positionSeconds == state.decks[0].positionSeconds
              && loaded->decks[0].playbackRate == state.decks[0].playbackRate
              && loaded->decks[0].headphoneCue
              && loaded->decks[0].wholeTrackLoop
              && loaded->decks[0].wasPlaying,
          "restore deck session state");
    check(loaded->decks[3].path == state.decks[3].path,
          "restore independent fourth deck state");

    auto invalid = state;
    invalid.decks[0].playbackRate = std::numeric_limits<float>::quiet_NaN();
    check(!store.save(sessionPath, invalid, &error), "reject non-finite state");
    const auto afterRejectedSave = store.load(sessionPath, &error);
    check(afterRejectedSave && afterRejectedSave->decks[0].path == state.decks[0].path,
          "rejected save preserves prior snapshot");

    const auto futurePath = temp.path / "future-session.bds";
    std::filesystem::copy_file(sessionPath, futurePath);
    {
        std::fstream file(futurePath, std::ios::in | std::ios::out | std::ios::binary);
        check(static_cast<bool>(file), "open future session fixture");
        file.seekp(8, std::ios::beg);
        const char futureVersion = 2;
        file.write(&futureVersion, 1);
    }
    check(!store.load(futurePath, &error), "future session schema rejected");

    const auto corruptPath = temp.path / "corrupt-session.bds";
    std::filesystem::copy_file(sessionPath, corruptPath);
    {
        std::fstream file(corruptPath, std::ios::in | std::ios::out | std::ios::binary);
        file.seekp(-1, std::ios::end);
        const char corrupted = static_cast<char>(0x5a);
        file.write(&corrupted, 1);
    }
    check(!store.load(corruptPath, &error), "checksum detects corruption");

    auto oversized = state;
    oversized.decks[0].path.assign(broke::session::SessionStore::maxPathBytes + 1, 'x');
    check(!store.save(temp.path / "oversized.bds", oversized, &error), "oversized path rejected");

    const auto interrupted = temp.path / "interrupted.bds";
    check(store.save(interrupted, state, &error), "prepare recovery fixture");
    auto backup = interrupted; backup += ".bak";
    std::filesystem::rename(interrupted, backup);
    bool usedBackup = false;
    const auto recovered = store.loadRecoveringBackup(interrupted, &usedBackup, &error);
    check(recovered && usedBackup && recovered->decks[0].path == state.decks[0].path,
          "verified backup recovery");

    writeBytes(interrupted, "corrupt-primary");
    usedBackup = false;
    const auto recoveredAgain = store.loadRecoveringBackup(interrupted, &usedBackup, &error);
    check(recoveredAgain && usedBackup && recoveredAgain->mixer.master == state.mixer.master,
          "corrupt primary falls back only to verified backup");
}

} // namespace

int main() {
    try {
        testLibraryWorkflowAndSafeRestore();
        testAutomaticContentHashing();
        testSyntheticLargeLibraryQueries();
        testMigrationAndFutureSchema();
        testSessionPersistence();
        std::cout << "Library/session persistence tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Library/session persistence tests failed: " << error.what() << '\n';
        return 1;
    }
}
