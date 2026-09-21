    check(!store.load(corruptPath, &error), "session checksum detects corruption");

    auto oversized = state;
    oversized.decks[0].path.assign(broke::session::SessionStore::maxPathBytes + 1, 'x');
    check(!store.save(temp.path / "oversized.bds", oversized, &error),
          "oversized session path rejected before write");

    const auto interruptedPath = temp.path / "interrupted-session.bds";
    check(store.save(interruptedPath, state, &error), "prepare interrupted session fixture");
    auto interruptedBackup = interruptedPath;
    interruptedBackup += ".bak";
    std::filesystem::rename(interruptedPath, interruptedBackup);
    bool usedBackup = false;
    const auto recovered = store.loadRecoveringBackup(interruptedPath, &usedBackup, &error);
    check(recovered && usedBackup && recovered->decks[0].path == state.decks[0].path,
          "explicit recovery loads verified backup after interrupted publish");

    std::ofstream badPrimary(interruptedPath, std::ios::binary | std::ios::trunc);
    badPrimary << "corrupt-primary";
    badPrimary.close();
    usedBackup = false;
    const auto recoveredFromCorruptPrimary = store.loadRecoveringBackup(interruptedPath, &usedBackup, &error);
    check(recoveredFromCorruptPrimary && usedBackup
              && recoveredFromCorruptPrimary->mixer.master == state.mixer.master,
          "explicit recovery bypasses corrupt primary only when backup validates");
}

} // namespace

int main() {
    try {
        testLibraryWorkflow();
        testAutomaticContentHashing();
        testSyntheticLargeLibraryQueries();
        testMigration();
        testFutureSchemaRejected();
        testSessionPersistence();
        std::cout << "Library/session persistence tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Library/session persistence tests failed: " << error.what() << '\n';
        return 1;
    }
}
