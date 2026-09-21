    check(db.relocateTrack(*firstId, moved, movedSize, 222, &error),
          "relocation rehashes selected file");
    duplicates = db.search(broke::library::LibraryDatabase::duplicateSearchDirective, 10, &error);
    check(duplicates.empty(), "relocation content change removes stale duplicate identity");

    const auto movedResult = db.search("moved", 10, &error);
    check(movedResult.size() == 1 && movedResult.front().contentHash.size() == 64
              && movedResult.front().contentHash != abcSha256,
          "relocated track stores refreshed SHA-256");
}

void testSyntheticLargeLibraryQueries() {
    TempDirectory temp;
    std::string error;
    broke::library::LibraryDatabase db;
    check(db.open(temp.path / "large-library.sqlite3", &error), "open synthetic large library");

    constexpr int trackCount = 1500;
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < trackCount; ++i) {
        auto value = track("virtual/track-" + std::to_string(i) + ".wav",
                           "Track " + std::to_string(i), "Stress Artist",
                           i % 300 == 0 ? "stress-duplicate" : "hash-" + std::to_string(i));
        if (i == trackCount - 1) value.title = "Needle Track 1499";
        check(db.upsertTrack(value, &error).has_value(), "populate synthetic large library");
    }

    const auto bounded = db.search("", 100, &error);
    check(bounded.size() == 100, "large-library search respects result bound");
    const auto needle = db.search("Needle Track 1499", 10, &error);
    check(needle.size() == 1, "large-library targeted search resolves final row");
    const auto duplicates = db.search(broke::library::LibraryDatabase::duplicateSearchDirective,
                                      100, &error);
    check(duplicates.size() == 5, "large-library duplicate directive remains bounded and correct");

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    std::cout << "Synthetic library diagnostic: " << trackCount
              << " rows inserted and queried in " << elapsed << " ms\n";
}

void testMigration() {
    TempDirectory temp;
    const auto path = temp.path / "legacy.sqlite3";
    createVersionOneDatabase(path);

    std::string error;
    broke::library::LibraryDatabase db;
    check(db.open(path, &error), "open legacy database");
    check(db.schemaVersion() == broke::library::LibraryDatabase::currentSchemaVersion,
          "legacy database migrated");
    const auto tracks = db.search("Legacy", 10, &error);
    check(tracks.size() == 1, "migration preserves track");
    check(tracks.front().contentHash.empty() && !tracks.front().missing,
          "migration applies v2 defaults");
    check(db.integrityCheck(&error), "migrated database integrity");
}

void testFutureSchemaRejected() {
    TempDirectory temp;
    const auto path = temp.path / "future.sqlite3";
    createFutureDatabase(path);

    std::string error;
    broke::library::LibraryDatabase db;
    check(!db.open(path, &error), "future schema must fail closed");
    check(!db.isOpen(), "failed future schema does not remain open");
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
    check(store.save(sessionPath, state, &error), "save session snapshot");
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
    check(!store.save(sessionPath, invalid, &error), "non-finite session state rejected");
    const auto afterRejectedSave = store.load(sessionPath, &error);
    check(afterRejectedSave && afterRejectedSave->decks[0].path == state.decks[0].path,
          "rejected save leaves previous session intact");

    const auto futurePath = temp.path / "future-session.bds";
    std::filesystem::copy_file(sessionPath, futurePath);
    {
        std::fstream file(futurePath, std::ios::in | std::ios::out | std::ios::binary);
        check(static_cast<bool>(file), "open future session fixture");
        file.seekp(8, std::ios::beg);
        const char futureVersion = 2;
        file.write(&futureVersion, 1);
        check(static_cast<bool>(file), "write future session fixture");
    }
    check(!store.load(futurePath, &error), "future session schema rejected");

    const auto corruptPath = temp.path / "corrupt-session.bds";
    std::filesystem::copy_file(sessionPath, corruptPath);
    {
        std::fstream file(corruptPath, std::ios::in | std::ios::out | std::ios::binary);
        check(static_cast<bool>(file), "open corrupt session fixture");
        file.seekp(-1, std::ios::end);
        const char corruptedByte = static_cast<char>(0x5a);
        file.write(&corruptedByte, 1);
        check(static_cast<bool>(file), "write corrupt session fixture");
    }
