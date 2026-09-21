    auto beta = track("music/Beta.wav", "Beta", "Bob", "sha256-B");
    auto alphaCopy = track("music/Alpha Copy.wav", "Alpha Copy", "Alice", "sha256-A");

    const auto alphaId = db.upsertTrack(alpha, &error);
    const auto betaId = db.upsertTrack(beta, &error);
    const auto alphaCopyId = db.upsertTrack(alphaCopy, &error);
    check(alphaId && betaId && alphaCopyId, "insert tracks");

    alpha.album = "Updated Album";
    const auto alphaIdAgain = db.upsertTrack(alpha, &error);
    check(alphaIdAgain && *alphaIdAgain == *alphaId, "upsert keeps stable track id");

    check(db.addTag(*alphaId, "Techno", &error), "add first tag");
    check(db.addTag(*alphaId, "Peak Time", &error), "add second tag");
    check(db.addTag(*alphaId, "TECHNO", &error), "case-insensitive tag remains idempotent");
    const auto tags = db.tagsForTrack(*alphaId, &error);
    check(tags.size() == 2, "tag set remains unique");

    const auto tagSearch = db.search("peak", 20, &error);
    check(tagSearch.size() == 1 && tagSearch.front().id == *alphaId,
          "search resolves tag text");
    const auto artistSearch = db.search("alice", 20, &error);
    check(artistSearch.size() == 2, "search resolves artist text");

    const auto playlist = db.createPlaylist("Tonight", &error);
    check(playlist.has_value(), "create playlist");
    check(db.addToPlaylist(*playlist, *alphaId, &error), "playlist add alpha");
    check(db.addToPlaylist(*playlist, *betaId, &error), "playlist add beta");
    check(db.addToPlaylist(*playlist, *alphaId, &error), "playlist duplicate is idempotent");
    auto playlistTracks = db.playlistTracks(*playlist, &error);
    check(playlistTracks.size() == 2 && playlistTracks[0].id == *alphaId
              && playlistTracks[1].id == *betaId,
          "playlist keeps insertion order");
    check(db.removeFromPlaylist(*playlist, *betaId, &error), "playlist remove");
    playlistTracks = db.playlistTracks(*playlist, &error);
    check(playlistTracks.size() == 1 && playlistTracks.front().id == *alphaId,
          "playlist remove preserves remaining item");

    check(db.recordPlay(*alphaId, 1000, &error), "history alpha");
    check(db.recordPlay(*betaId, 2000, &error), "history beta");
    const auto history = db.recentHistory(10, &error);
    check(history.size() == 2 && history.front().trackId == *betaId
              && history.back().trackId == *alphaId,
          "history sorts newest first");

    auto duplicates = db.duplicateGroups(32, &error);
    check(duplicates.size() == 1 && duplicates.front().contentHash == "sha256-A"
              && duplicates.front().tracks.size() == 2,
          "content hash finds duplicate group");
    const auto duplicateSearch = db.search(broke::library::LibraryDatabase::duplicateSearchDirective,
                                           32, &error);
    check(duplicateSearch.size() == 2, "duplicate directive exposes duplicate candidates");

    check(db.markMissing(*alphaCopyId, true, &error), "mark duplicate missing");
    duplicates = db.duplicateGroups(32, &error);
    check(duplicates.empty(), "missing file excluded from duplicate candidates");
    const auto missingSearch = db.search(broke::library::LibraryDatabase::missingSearchDirective,
                                         32, &error);
    check(missingSearch.size() == 1 && missingSearch.front().id == *alphaCopyId,
          "missing directive exposes disconnected tracks");
    check(db.markMissing(*alphaCopyId, false, &error), "restore duplicate presence");

    check(db.markMissing(*alphaId, true, &error), "mark source missing");
    const auto relocatedPath = temp.path / "moved" / "Alpha.wav";
    writeBytes(relocatedPath, "relocated-alpha");
    const auto relocatedSize = static_cast<std::int64_t>(std::filesystem::file_size(relocatedPath));
    check(db.relocateTrack(*alphaId, relocatedPath, relocatedSize, 123456789, &error),
          "relocate source");
    const auto relocated = db.search("Alpha.wav", 20, &error);
    bool relocatedFound = false;
    for (const auto& item : relocated) {
        if (item.id == *alphaId) {
            relocatedFound = item.path.find("moved/Alpha.wav") != std::string::npos
                && !item.missing && item.fileSize == relocatedSize
                && item.contentHash.size() == 64;
        }
    }
    check(relocatedFound, "relocation preserves id and refreshes content identity");
    check(db.tagsForTrack(*alphaId, &error).size() == 2,
          "relocation preserves tag relationships");
    check(db.playlistTracks(*playlist, &error).front().id == *alphaId,
          "relocation preserves playlist relationships");

    const auto backup = temp.path / "backup.sqlite3";
    check(db.backupTo(backup, &error), "backup library");
    auto afterBackup = track("music/After Backup.wav", "After Backup", "Carol", "sha256-C");
    check(db.upsertTrack(afterBackup, &error).has_value(), "mutate after backup");
    check(db.search("", 100, &error).size() == 4, "post-backup mutation visible");
    check(db.restoreFrom(backup, &error), "restore library");
    check(db.search("", 100, &error).size() == 3, "restore returns backup snapshot");
    check(db.integrityCheck(&error), "restored database integrity");

    const auto corrupt = temp.path / "corrupt.sqlite3";
    {
        std::ofstream output(corrupt, std::ios::binary);
        output << "not a sqlite database";
    }
    check(!db.restoreFrom(corrupt, &error), "corrupt restore rejected before replacement");
    check(db.search("", 100, nullptr).size() == 3, "failed restore leaves live database intact");

    const auto futureBackup = temp.path / "future-backup.sqlite3";
    createFutureDatabase(futureBackup);
    check(!db.restoreFrom(futureBackup, &error), "future backup schema rejected before replacement");
    check(db.search("", 100, nullptr).size() == 3,
          "future backup rejection leaves live database intact");
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

    auto firstTrack = track(first.generic_string(), "First", "Fixture", "");
    firstTrack.fileSize = 3;
    auto secondTrack = track(second.generic_string(), "Second", "Fixture", "");
    secondTrack.fileSize = 3;
    const auto firstId = db.upsertTrack(firstTrack, &error);
    const auto secondId = db.upsertTrack(secondTrack, &error);
    check(firstId && secondId, "hash local imports");

    const auto rows = db.search("Fixture", 10, &error);
    check(rows.size() == 2, "hashed imports remain searchable");
    constexpr std::string_view abcSha256 =
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    check(rows[0].contentHash == abcSha256 && rows[1].contentHash == abcSha256,
          "local import hash matches SHA-256 reference vector");

    auto duplicates = db.search(broke::library::LibraryDatabase::duplicateSearchDirective,
                                10, &error);
    check(duplicates.size() == 2, "automatic hashes feed duplicate review");

    const auto moved = temp.path / "moved" / "first.wav";
    writeBytes(moved, "different-content");
    const auto movedSize = static_cast<std::int64_t>(std::filesystem::file_size(moved));
