// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "LibraryPanel.h"
#include "MainComponent.h"

#include <JuceHeader.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <utility>

// Native, local-only M4 library/session workflow. Database/session/file work is
// performed by bounded worker pools and never from MainComponent::getNextAudioBlock().
class LibraryWorkflow final : private juce::Timer {
public:
    explicit LibraryWorkflow(MainComponent& target) : owner(target) {
        lifetime = std::make_shared<int>(0);
        button.setButtonText(uiText("Library", "Biblioteka"));
        button.setTooltip(uiText(
            "Local library and session commands. Music paths and session data stay on this PC; no cloud account is used.",
            "Lokalna biblioteka i polecenia sesji. Ścieżki muzyki i dane sesji zostają na tym komputerze; konto w chmurze nie jest używane."));
        button.onClick = [this] { showCommandMenu(); };
        owner.addAndMakeVisible(button);

        const auto databaseFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                      .getChildFile("BrokeDJ")
                                      .getChildFile("library.sqlite3");
        std::string error;
        databaseAvailable = database.open(filesystemPath(databaseFile), &error);
        if (!databaseAvailable) {
            button.setTooltip(uiText(
                "The local library database could not be opened. Direct deck loading and session save/load remain available.",
                "Nie udało się otworzyć lokalnej bazy biblioteki. Bezpośrednie wczytywanie decków oraz zapis/odczyt sesji nadal działają."));
        }
    }

    ~LibraryWorkflow() override {
        stopTimer();
        cancelled.store(true, std::memory_order_release);
        lifetime.reset();
        chooser.reset();
        sessionChooser.reset();
        if (dialog) delete dialog.getComponent();
        searchWorkers.removeAllJobs(true, -1);
        writeWorkers.removeAllJobs(true, -1);
        database.close();
    }

    LibraryWorkflow(const LibraryWorkflow&) = delete;
    LibraryWorkflow& operator=(const LibraryWorkflow&) = delete;

    void setButtonBounds(juce::Rectangle<int> bounds) { button.setBounds(bounds); }

private:
    struct PendingSessionDeck final {
        bool active = false;
        juce::File file;
        broke::session::DeckState state;
        int ticks = 0;
    };

    static juce::String uiText(const char* english, const char* polish) {
        static const bool usePolish = juce::SystemStats::getUserLanguage().startsWithIgnoreCase("pl");
        return juce::String::fromUTF8(usePolish ? polish : english);
    }

    static std::filesystem::path filesystemPath(const juce::File& file) {
#if JUCE_WINDOWS
        return std::filesystem::path(file.getFullPathName().toWideCharPointer());
#else
        return std::filesystem::u8path(file.getFullPathName().toStdString());
#endif
    }

    static std::string utf8Path(const juce::File& file) {
        const auto value = filesystemPath(file).lexically_normal().generic_u8string();
        return std::string(reinterpret_cast<const char*>(value.data()), value.size());
    }

    static std::int64_t modifiedNs(const juce::File& file) {
        const auto ms = file.getLastModificationTime().toMilliseconds();
        return ms > 0 && ms <= std::numeric_limits<std::int64_t>::max() / 1000000
            ? ms * 1000000 : ms;
    }

    void showCommandMenu() {
        juce::PopupMenu menu;
        menu.addItem(1, uiText("Open local library", "Otwórz lokalną bibliotekę"),
                     databaseAvailable && !restoreInProgress);
        menu.addSeparator();
        menu.addItem(2, uiText("Save session…", "Zapisz sesję…"), !restoreInProgress && !sessionChooser);
        menu.addItem(3, uiText("Load session…", "Wczytaj sesję…"), !restoreInProgress && !sessionChooser);
        menu.addItem(4, uiText("Recover verified session backup…", "Odzyskaj zweryfikowaną kopię sesji…"),
                     !restoreInProgress && !sessionChooser);
        const auto weak = std::weak_ptr<int>(lifetime);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&button),
                           [this, weak](int result) {
            if (weak.expired()) return;
            switch (result) {
                case 1: show(); break;
                case 2: saveSession(); break;
                case 3: loadSession(false); break;
                case 4: loadSession(true); break;
                default: break;
            }
        });
    }

    void show() {
        if (!databaseAvailable || !database.isOpen()) return;
        if (dialog) {
            dialog->toFront(true);
            return;
        }

        auto* panelComponent = new LibraryPanel();
        const auto weak = std::weak_ptr<int>(lifetime);
        panelComponent->onSearchChanged = [this, weak](const juce::String& query) {
            if (!weak.expired()) queueSearch(query);
        };
        panelComponent->onImportFiles = [this, weak] {
            if (!weak.expired()) importFiles();
        };
        panelComponent->onLoadTrack = [this, weak](const broke::library::TrackRecord& track, std::size_t deck) {
            if (!weak.expired()) loadTrack(track, deck);
        };
        panelComponent->onRelocateTrack = [this, weak](const broke::library::TrackRecord& track) {
            if (!weak.expired()) relocateTrack(track);
        };

        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = uiText("BrokeDJ / Local library", "BrokeDJ / Lokalna biblioteka");
        options.dialogBackgroundColour = juce::Colour(0xff080e1a);
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.escapeKeyTriggersCloseButton = true;
        options.content.setOwned(panelComponent);
        options.content->setSize(820, 540);
        options.componentToCentreAround = &owner;
        panel = panelComponent;
        dialog = options.launchAsync();
        queueSearch({});
    }

    void queueSearch(const juce::String& query) {
        if (!database.isOpen() || cancelled.load(std::memory_order_acquire)) return;
        const auto generation = queryGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
        if (panel) panel->setBusy(true);

        // Bound type-ahead work: one currently-running query plus at most the
        // latest pending query. Import/write jobs use a separate single worker.
        searchWorkers.removeAllJobs(false, 0);
        const auto queryUtf8 = query.toStdString();
        const auto weak = std::weak_ptr<int>(lifetime);
        searchWorkers.addJob([this, weak, generation, queryUtf8] {
            if (weak.expired() || cancelled.load(std::memory_order_acquire)) return;
            std::string error;
            auto results = database.search(queryUtf8, 500, &error);
            const bool ok = error.empty();
            juce::MessageManager::callAsync(
                [this, weak, generation, results = std::move(results), ok]() mutable {
                    if (weak.expired() || cancelled.load(std::memory_order_acquire)
                        || queryGeneration.load(std::memory_order_acquire) != generation || !panel)
                        return;
                    if (!ok) {
                        panel->setBusy(false);
                        panel->setMessage(uiText("Library search failed.",
                                                 "Wyszukiwanie biblioteki nie powiodło się."));
                        return;
                    }
                    panel->setResults(std::move(results));
                });
        });
    }

    void importFiles() {
        if (chooser || !database.isOpen()) return;
        chooser = std::make_unique<juce::FileChooser>(
            uiText("Import local music", "Importuj lokalną muzykę"), juce::File{},
            "*.wav;*.aiff;*.aif;*.flac;*.ogg;*.mp3");
        const auto weak = std::weak_ptr<int>(lifetime);
        chooser->launchAsync(juce::FileBrowserComponent::openMode
                                 | juce::FileBrowserComponent::canSelectFiles
                                 | juce::FileBrowserComponent::canSelectMultipleItems,
            [this, weak](const juce::FileChooser& chooserRef) {
                if (weak.expired()) return;
                auto files = chooserRef.getResults();
                chooser.reset();
                if (files.isEmpty()) return;
                constexpr int maxImportBatch = 5000;
                if (files.size() > maxImportBatch)
                    files.removeRange(maxImportBatch, files.size() - maxImportBatch);
                if (panel) panel->setBusy(true);

                writeWorkers.addJob([this, weak, files = std::move(files)] {
                    if (weak.expired()) return;
                    int imported = 0;
                    for (const auto& file : files) {
                        if (cancelled.load(std::memory_order_acquire)) return;
                        if (!file.existsAsFile()) continue;
                        broke::library::TrackRecord record;
                        record.path = utf8Path(file);
                        record.fileSize = std::max<std::int64_t>(0, file.getSize());
                        record.modifiedNs = modifiedNs(file);
                        record.title = file.getFileNameWithoutExtension().toStdString();
                        std::string error;
                        if (database.upsertTrack(record, &error)) ++imported;
                    }
                    juce::MessageManager::callAsync([this, weak, imported] {
                        if (weak.expired() || cancelled.load(std::memory_order_acquire)) return;
                        if (panel) {
                            panel->setMessage(uiText("Imported/updated tracks: ",
                                                     "Zaimportowano/zaktualizowano utwory: ")
                                              + juce::String(imported));
                            queueSearch(panel->query());
                        }
                    });
                });
            });
    }

    void loadTrack(const broke::library::TrackRecord& track, std::size_t deck) {
        if (deck >= broke::deckCount) return;
        const juce::File file(juce::String::fromUTF8(track.path.c_str()));
        if (!file.existsAsFile()) {
            const auto trackId = track.id;
            const auto weak = std::weak_ptr<int>(lifetime);
            writeWorkers.addJob([this, weak, trackId] {
                if (weak.expired() || cancelled.load(std::memory_order_acquire)) return;
                static_cast<void>(database.markMissing(trackId, true, nullptr));
                juce::MessageManager::callAsync([this, weak] {
                    if (weak.expired() || cancelled.load(std::memory_order_acquire)) return;
                    if (panel) {
                        panel->setMessage(uiText("Track file is missing. Use Relocate to reconnect it.",
                                                 "Brakuje pliku utworu. Użyj opcji Wskaż nowy plik."));
                        queueSearch(panel->query());
                    }
                });
            });
            return;
        }
        static_cast<void>(owner.loadFileIntoDeck(deck, file));
    }

    void relocateTrack(const broke::library::TrackRecord& track) {
        if (chooser || !database.isOpen()) return;
        chooser = std::make_unique<juce::FileChooser>(
            uiText("Relocate library track", "Wskaż nowy plik utworu"), juce::File{},
            "*.wav;*.aiff;*.aif;*.flac;*.ogg;*.mp3");
        const auto trackId = track.id;
        const auto weak = std::weak_ptr<int>(lifetime);
        chooser->launchAsync(juce::FileBrowserComponent::openMode
                                 | juce::FileBrowserComponent::canSelectFiles,
            [this, weak, trackId](const juce::FileChooser& chooserRef) {
                if (weak.expired()) return;
                const auto file = chooserRef.getResult();
                chooser.reset();
                if (!file.existsAsFile()) return;
                if (panel) panel->setBusy(true);
                writeWorkers.addJob([this, weak, file, trackId] {
                    if (weak.expired() || cancelled.load(std::memory_order_acquire)) return;
                    std::string error;
                    const bool ok = database.relocateTrack(
                        trackId, filesystemPath(file), std::max<std::int64_t>(0, file.getSize()),
                        modifiedNs(file), &error);
                    juce::MessageManager::callAsync([this, weak, ok] {
                        if (weak.expired() || cancelled.load(std::memory_order_acquire)) return;
                        if (panel) {
                            panel->setMessage(ok
                                ? uiText("Library track reconnected.",
                                         "Ponownie połączono utwór biblioteki.")
                                : uiText("Could not relocate library track.",
                                         "Nie udało się wskazać nowego pliku utworu."));
                            queueSearch(panel->query());
                        }
                    });
                });
            });
    }

    void saveSession() {
        if (restoreInProgress || sessionChooser) return;
        auto directory = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        if (!directory.isDirectory())
            directory = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
        const auto stamp = juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S");
        const auto suggested = directory.getChildFile("BrokeDJ-session-" + stamp + ".brokedj-session");
        sessionChooser = std::make_unique<juce::FileChooser>(
            uiText("Save BrokeDJ session", "Zapisz sesję BrokeDJ"), suggested,
            "*.brokedj-session");
        const auto weak = std::weak_ptr<int>(lifetime);
        sessionChooser->launchAsync(
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                | juce::FileBrowserComponent::warnAboutOverwriting,
            [this, weak](const juce::FileChooser& chooserRef) {
                if (weak.expired()) return;
                auto selected = chooserRef.getResult();
                sessionChooser.reset();
                if (selected == juce::File{}) return;
                if (selected.getFileExtension().toLowerCase() != ".brokedj-session")
                    selected = selected.withFileExtension("brokedj-session");
                const auto snapshot = owner.captureSessionState();
                const auto weakAgain = std::weak_ptr<int>(lifetime);
                writeWorkers.addJob([this, weakAgain, selected, snapshot] {
                    if (weakAgain.expired() || cancelled.load(std::memory_order_acquire)) return;
                    std::string error;
                    const bool ok = sessionStore.save(filesystemPath(selected), snapshot, &error);
                    juce::MessageManager::callAsync([this, weakAgain, selected, ok] {
                        if (weakAgain.expired() || cancelled.load(std::memory_order_acquire)) return;
                        owner.showWorkflowStatus(ok
                            ? uiText("Session saved: ", "Sesja zapisana: ") + selected.getFileName()
                            : uiText("Session save failed.", "Zapis sesji nie powiódł się."));
                        if (!ok) {
                            juce::AlertWindow::showMessageBoxAsync(
                                juce::MessageBoxIconType::WarningIcon,
                                uiText("Session save failed", "Zapis sesji nie powiódł się"),
                                uiText("BrokeDJ could not safely publish the session file. Existing music files were not modified.",
                                       "BrokeDJ nie mógł bezpiecznie zapisać pliku sesji. Oryginalne pliki muzyczne nie zostały zmienione."));
                        }
                    });
                });
            });
    }

    void loadSession(bool recoverBackup) {
        if (restoreInProgress || sessionChooser) return;
        auto directory = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        sessionChooser = std::make_unique<juce::FileChooser>(
            recoverBackup
                ? uiText("Select session or .bak to recover", "Wybierz sesję lub .bak do odzyskania")
                : uiText("Load BrokeDJ session", "Wczytaj sesję BrokeDJ"),
            directory, recoverBackup ? "*.brokedj-session;*.bak" : "*.brokedj-session");
        const auto weak = std::weak_ptr<int>(lifetime);
        sessionChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, weak, recoverBackup](const juce::FileChooser& chooserRef) {
                if (weak.expired()) return;
                auto selected = chooserRef.getResult();
                sessionChooser.reset();
                if (!selected.existsAsFile()) return;

                auto primary = selected;
                if (recoverBackup && selected.getFullPathName().endsWithIgnoreCase(".bak")) {
                    const auto path = selected.getFullPathName();
                    primary = juce::File(path.dropLastCharacters(4));
                }
                const auto weakAgain = std::weak_ptr<int>(lifetime);
                writeWorkers.addJob([this, weakAgain, primary, recoverBackup] {
                    if (weakAgain.expired() || cancelled.load(std::memory_order_acquire)) return;
                    std::string error;
                    bool usedBackup = false;
                    auto state = recoverBackup
                        ? sessionStore.loadRecoveringBackup(filesystemPath(primary), &usedBackup, &error)
                        : sessionStore.load(filesystemPath(primary), &error);
                    juce::MessageManager::callAsync(
                        [this, weakAgain, primary, state = std::move(state), usedBackup]() mutable {
                            if (weakAgain.expired() || cancelled.load(std::memory_order_acquire)) return;
                            if (!state) {
                                owner.showWorkflowStatus(uiText("Session load failed.", "Wczytanie sesji nie powiodło się."));
                                juce::AlertWindow::showMessageBoxAsync(
                                    juce::MessageBoxIconType::WarningIcon,
                                    uiText("Session load failed", "Wczytanie sesji nie powiodło się"),
                                    uiText("The selected snapshot failed validation. Current decks were left unchanged.",
                                           "Wybrany zapis nie przeszedł walidacji. Bieżące decki pozostały bez zmian."));
                                return;
                            }
                            beginSessionRestore(*state, primary, usedBackup);
                        });
                });
            });
    }

    void beginSessionRestore(const broke::session::SessionState& state,
                             const juce::File& source, bool usedBackup) {
        restoreInProgress = true;
        restoredDecks = 0;
        missingDecks = 0;
        failedDecks = 0;
        emptySlots = 0;
        restoredFromBackup = usedBackup;
        restoredSessionName = source.getFileName();
        for (auto& pending : pendingSessionDecks) pending = {};

        // Safety-first restore: stop all current transports and restore mixer
        // controls, but never auto-resume a saved playing deck. Each source is
        // decoded through the existing async loader before position/controls apply.
        owner.prepareForSessionRestore(state.mixer);
        for (std::size_t deck = 0; deck < broke::session::SessionState::deckCount; ++deck) {
            const auto& deckState = state.decks[deck];
            if (deckState.path.empty()) {
                ++emptySlots;
                continue;
            }
            const juce::File file(juce::String::fromUTF8(deckState.path.c_str()));
            if (!file.existsAsFile()) {
                ++missingDecks;
                continue;
            }
            if (!owner.loadFileIntoDeck(deck, file)) {
                ++failedDecks;
                continue;
            }
            auto& pending = pendingSessionDecks[deck];
            pending.active = true;
            pending.file = file;
            pending.state = deckState;
        }

        owner.showWorkflowStatus(uiText("Restoring session safely; playback stays paused…",
                                        "Bezpieczne odtwarzanie sesji; playback pozostaje wstrzymany…"));
        if (pendingCount() == 0) {
            finishSessionRestore();
            return;
        }
        startTimerHz(20);
    }

    void timerCallback() override {
        if (!restoreInProgress) {
            stopTimer();
            return;
        }
        for (std::size_t deck = 0; deck < pendingSessionDecks.size(); ++deck) {
            auto& pending = pendingSessionDecks[deck];
            if (!pending.active) continue;
            ++pending.ticks;

            const bool matches = owner.sessionDeckMatches(deck, pending.file);
            const bool loadingNow = owner.sessionDeckLoading(deck);
            if (matches && owner.sessionDeckDuration(deck) > 0.0) {
                const bool applied = owner.applyRestoredDeckState(deck, pending.state);
                pending.active = false;
                if (applied) ++restoredDecks;
                else ++failedDecks;
                continue;
            }

            // Decode/import failure leaves the previously loaded deck untouched.
            // If the expected file was never published and loading has ended,
            // fail that slot rather than applying saved controls to the wrong clip.
            if (!loadingNow && !matches && pending.ticks > 4) {
                pending.active = false;
                ++failedDecks;
                continue;
            }

            // A valid clip is adopted by the audio callback. If no working audio
            // device is available, do not spin forever or pretend restore completed.
            if (pending.ticks > 300) {
                pending.active = false;
                ++failedDecks;
            }
        }
        if (pendingCount() == 0) finishSessionRestore();
    }

    [[nodiscard]] int pendingCount() const noexcept {
        return static_cast<int>(std::count_if(
            pendingSessionDecks.begin(), pendingSessionDecks.end(),
            [](const PendingSessionDeck& pending) { return pending.active; }));
    }

    void finishSessionRestore() {
        stopTimer();
        restoreInProgress = false;
        juce::String detail = uiText("Restored decks: ", "Przywrócone decki: ")
            + juce::String(restoredDecks)
            + uiText(" / missing files: ", " / brakujące pliki: ") + juce::String(missingDecks)
            + uiText(" / failed slots: ", " / nieudane sloty: ") + juce::String(failedDecks);
        if (emptySlots > 0)
            detail += uiText(" / empty saved slots: ", " / puste zapisane sloty: ") + juce::String(emptySlots);
        detail += uiText(". Playback remains paused after restore.",
                         ". Playback pozostaje wstrzymany po odtworzeniu sesji.");
        if (emptySlots > 0)
            detail += uiText(" Empty saved slots do not eject an already loaded source yet.",
                             " Puste zapisane sloty nie usuwają jeszcze wcześniej wczytanego źródła.");
        if (restoredFromBackup)
            detail += uiText(" Verified .bak recovery was used.", " Użyto zweryfikowanej kopii .bak.");

        const bool clean = missingDecks == 0 && failedDecks == 0;
        owner.showWorkflowStatus(clean
            ? uiText("Session restored (paused): ", "Sesja przywrócona (pauza): ") + restoredSessionName
            : uiText("Session restored with missing/failed decks.",
                     "Sesja przywrócona z brakującymi/nieudanymi deckami."));
        juce::AlertWindow::showMessageBoxAsync(
            clean ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
            clean ? uiText("Session restored", "Sesja przywrócona")
                  : uiText("Session restore needs attention", "Przywrócenie sesji wymaga uwagi"),
            detail);
    }

    MainComponent& owner;
    juce::TextButton button;
    broke::library::LibraryDatabase database;
    broke::session::SessionStore sessionStore;
    juce::ThreadPool searchWorkers{1};
    juce::ThreadPool writeWorkers{1};
    std::atomic<bool> cancelled{false};
    std::atomic<std::uint64_t> queryGeneration{0};
    std::shared_ptr<int> lifetime;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::FileChooser> sessionChooser;
    juce::Component::SafePointer<juce::DialogWindow> dialog;
    juce::Component::SafePointer<LibraryPanel> panel;
    bool databaseAvailable = false;

    std::array<PendingSessionDeck, broke::session::SessionState::deckCount> pendingSessionDecks{};
    bool restoreInProgress = false;
    bool restoredFromBackup = false;
    int restoredDecks = 0;
    int missingDecks = 0;
    int failedDecks = 0;
    int emptySlots = 0;
    juce::String restoredSessionName;
};
