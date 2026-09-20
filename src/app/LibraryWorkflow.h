// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "LibraryPanel.h"
#include "MainComponent.h"
#include "SessionStore.h"

#include <JuceHeader.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Native, local-only M4 library/session workflow. Database/session/file work is
// performed by bounded worker pools and never from MainComponent::getNextAudioBlock().
class LibraryWorkflow final {
public:
    explicit LibraryWorkflow(MainComponent& target) : owner(target) {
        lifetime = std::make_shared<int>(0);
        button.setButtonText(uiText("Library", "Biblioteka"));
        button.setTooltip(uiText("Open the local BrokeDJ music library and session controls. No cloud account is used.",
                                 "Otwórz lokalną bibliotekę muzyki i kontrolki sesji BrokeDJ. Konto w chmurze nie jest używane."));
        button.onClick = [this] { show(); };
        owner.addAndMakeVisible(button);

        const auto databaseFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                      .getChildFile("BrokeDJ")
                                      .getChildFile("library.sqlite3");
        std::string error;
        if (!database.open(filesystemPath(databaseFile), &error)) {
            button.setEnabled(false);
            button.setTooltip(uiText("Local library database could not be opened. Direct deck loading is still available.",
                                     "Nie udało się otworzyć lokalnej bazy biblioteki. Bezpośrednie wczytywanie decków nadal działa."));
        }
    }

    ~LibraryWorkflow() {
        cancelled.store(true, std::memory_order_release);
        restoreGeneration.fetch_add(1, std::memory_order_acq_rel);
        lifetime.reset();
        chooser.reset();
        sessionChooser.reset();
        if (dialog) delete dialog.getComponent();
        searchWorkers.removeAllJobs(true, -1);
        writeWorkers.removeAllJobs(true, -1);
        sessionWorkers.removeAllJobs(true, -1);
        database.close();
    }

    LibraryWorkflow(const LibraryWorkflow&) = delete;
    LibraryWorkflow& operator=(const LibraryWorkflow&) = delete;

    void setButtonBounds(juce::Rectangle<int> bounds) { button.setBounds(bounds); }

private:
    struct PendingSessionDeck final {
        std::size_t deck = 0;
        broke::session::DeckState state;
        juce::File file;
        double durationSeconds = 0.0;
        bool complete = false;
    };

    struct RestoreBatch final {
        std::uint64_t generation = 0;
        std::vector<PendingSessionDeck> decks;
        int skippedEmpty = 0;
        int missingFiles = 0;
        int unreadableFiles = 0;
        int busyDecks = 0;
        int restoredDecks = 0;
        int failedLoads = 0;
        int pollsRemaining = 300;
        bool usedBackup = false;
        juce::String sourceName;
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

    static double probeTrackDuration(const juce::File& file) {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (!reader || !std::isfinite(reader->sampleRate) || reader->sampleRate < 8000.0
            || reader->lengthInSamples <= 0) {
            return 0.0;
        }
        const double duration = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
        return std::isfinite(duration) && duration > 0.0 ? duration : 0.0;
    }

    static juce::File defaultSessionDirectory() {
        auto root = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        if (!root.isDirectory())
            root = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
        return root.getChildFile("BrokeDJ Sessions");
    }

    void show() {
        if (!database.isOpen()) return;
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
        panelComponent->onSaveSession = [this, weak] {
            if (!weak.expired()) saveSession();
        };
        panelComponent->onLoadSession = [this, weak] {
            if (!weak.expired()) chooseSessionToLoad(false);
        };
        panelComponent->onRecoverSession = [this, weak] {
            if (!weak.expired()) chooseSessionToLoad(true);
        };

        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = uiText("BrokeDJ / Local library", "BrokeDJ / Lokalna biblioteka");
        options.dialogBackgroundColour = juce::Colour(0xff080e1a);
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.escapeKeyTriggersCloseButton = true;
        options.content.setOwned(panelComponent);
        options.content->setSize(900, 600);
        options.componentToCentreAround = &owner;
        panel = panelComponent;
        dialog = options.launchAsync();
        queueSearch({});
    }

    void queueSearch(const juce::String& query) {
        if (!database.isOpen() || cancelled.load(std::memory_order_acquire)) return;
        const auto generation = queryGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
        if (panel) panel->setBusy(true);

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
        if (chooser || sessionChooser || !database.isOpen()) return;
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
        owner.loadFileIntoDeck(deck, file);
    }

    void relocateTrack(const broke::library::TrackRecord& track) {
        if (chooser || sessionChooser || !database.isOpen()) return;
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
        if (chooser || sessionChooser) return;
        if (!owner.sessionSnapshotReady()) {
            if (panel) panel->setMessage(uiText(
                "Wait until current track loading finishes before saving a session.",
                "Poczekaj na zakończenie wczytywania utworów przed zapisaniem sesji."));
            return;
        }

        auto directory = defaultSessionDirectory();
        const auto suggested = directory.getChildFile("BrokeDJ-session.brksession");
        sessionChooser = std::make_unique<juce::FileChooser>(
            uiText("Save BrokeDJ session", "Zapisz sesję BrokeDJ"),
            suggested, "*.brksession");
        const auto weak = std::weak_ptr<int>(lifetime);
        sessionChooser->launchAsync(juce::FileBrowserComponent::saveMode
                                        | juce::FileBrowserComponent::canSelectFiles
                                        | juce::FileBrowserComponent::warnAboutOverwriting,
            [this, weak](const juce::FileChooser& chooserRef) {
                if (weak.expired()) return;
                auto file = chooserRef.getResult();
                sessionChooser.reset();
                if (file == juce::File{}) return;
                if (!owner.sessionSnapshotReady()) {
                    if (panel) panel->setMessage(uiText(
                        "A track started loading; session save was cancelled to avoid an inconsistent snapshot.",
                        "Rozpoczęło się wczytywanie utworu; zapis sesji anulowano, aby uniknąć niespójnego stanu."));
                    return;
                }
                if (file.getFileExtension().toLowerCase() != ".brksession")
                    file = file.withFileExtension("brksession");

                const auto state = owner.captureSessionState();
                if (panel) panel->setBusy(true);
                sessionWorkers.addJob([this, weak, file, state] {
                    if (weak.expired() || cancelled.load(std::memory_order_acquire)) return;
                    broke::session::SessionStore store;
                    std::string error;
                    const bool ok = store.save(filesystemPath(file), state, &error);
                    juce::MessageManager::callAsync([this, weak, ok, name = file.getFileName()] {
                        if (weak.expired() || cancelled.load(std::memory_order_acquire)) return;
                        if (!panel) return;
                        panel->setBusy(false);
                        panel->setMessage(ok
                            ? uiText("Session saved: ", "Sesja zapisana: ") + name
                            : uiText("Session save failed. The previous verified session was not intentionally overwritten.",
                                     "Zapis sesji nie powiódł się. Poprzednia zweryfikowana sesja nie została celowo nadpisana."));
                    });
                });
            });
    }

    void chooseSessionToLoad(bool recoverBackup) {
        if (chooser || sessionChooser) return;
        if (!owner.sessionSnapshotReady()) {
            if (panel) panel->setMessage(uiText(
                "Wait until current track loading finishes before loading a session.",
                "Poczekaj na zakończenie wczytywania utworów przed wczytaniem sesji."));
            return;
        }

        sessionChooser = std::make_unique<juce::FileChooser>(
            recoverBackup
                ? uiText("Select primary session for backup recovery",
                         "Wybierz główną sesję do odzyskania kopii")
                : uiText("Load BrokeDJ session", "Wczytaj sesję BrokeDJ"),
            defaultSessionDirectory(), "*.brksession");
        const auto weak = std::weak_ptr<int>(lifetime);
        sessionChooser->launchAsync(juce::FileBrowserComponent::openMode
                                        | juce::FileBrowserComponent::canSelectFiles,
            [this, weak, recoverBackup](const juce::FileChooser& chooserRef) {
                if (weak.expired()) return;
                const auto file = chooserRef.getResult();
                sessionChooser.reset();
                if (!file.existsAsFile()) return;
                loadSessionFile(file, recoverBackup);
            });
    }

    void loadSessionFile(const juce::File& file, bool recoverBackup) {
        if (!owner.sessionSnapshotReady()) {
            if (panel) panel->setMessage(uiText(
                "A track started loading; session restore was cancelled before changing mixer state.",
                "Rozpoczęło się wczytywanie utworu; przywracanie sesji anulowano przed zmianą miksera."));
            return;
        }

        const auto generation = restoreGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
        if (panel) panel->setBusy(true);
        const auto weak = std::weak_ptr<int>(lifetime);
        sessionWorkers.addJob([this, weak, file, recoverBackup, generation] {
            if (weak.expired() || cancelled.load(std::memory_order_acquire)
                || restoreGeneration.load(std::memory_order_acquire) != generation) return;

            broke::session::SessionStore store;
            std::string error;
            bool usedBackup = false;
            auto state = recoverBackup
                ? store.loadRecoveringBackup(filesystemPath(file), &usedBackup, &error)
                : store.load(filesystemPath(file), &error);
            if (!state) {
                juce::MessageManager::callAsync([this, weak, generation, recoverBackup] {
                    if (weak.expired() || cancelled.load(std::memory_order_acquire)
                        || restoreGeneration.load(std::memory_order_acquire) != generation || !panel)
                        return;
                    panel->setBusy(false);
                    panel->setMessage(recoverBackup
                        ? uiText("Primary session and verified backup could not be loaded.",
                                 "Nie udało się wczytać głównej sesji ani zweryfikowanej kopii.")
                        : uiText("Session validation failed. No mixer/deck state was changed.",
                                 "Weryfikacja sesji nie powiodła się. Stan miksera/decków nie został zmieniony."));
                });
                return;
            }

            auto batch = std::make_shared<RestoreBatch>();
            batch->generation = generation;
            batch->usedBackup = usedBackup;
            batch->sourceName = file.getFileName();
            batch->decks.reserve(broke::deckCount);
            for (std::size_t deck = 0; deck < broke::deckCount; ++deck) {
                const auto& deckState = state->decks[deck];
                if (deckState.path.empty()) {
                    ++batch->skippedEmpty;
                    continue;
                }
                const juce::File trackFile(juce::String::fromUTF8(deckState.path.c_str()));
                if (!trackFile.existsAsFile()) {
                    ++batch->missingFiles;
                    continue;
                }
                const double duration = probeTrackDuration(trackFile);
                if (!(duration > 0.0)) {
                    ++batch->unreadableFiles;
                    continue;
                }
                PendingSessionDeck pending;
                pending.deck = deck;
                pending.state = deckState;
                pending.file = trackFile;
                pending.durationSeconds = duration;
                batch->decks.push_back(std::move(pending));
            }

            const auto mixerState = state->mixer;
            juce::MessageManager::callAsync(
                [this, weak, generation, batch, mixerState] {
                    if (weak.expired() || cancelled.load(std::memory_order_acquire)
                        || restoreGeneration.load(std::memory_order_acquire) != generation)
                        return;

                    owner.beginSessionRestore(mixerState);

                    for (auto& pending : batch->decks) {
                        if (owner.sessionDeckIsLoading(pending.deck)) {
                            pending.complete = true;
                            ++batch->busyDecks;
                            continue;
                        }
                        owner.loadFileIntoDeck(pending.deck, pending.file);
                    }
                    pollSessionRestore(batch);
                });
        });
    }

    void pollSessionRestore(const std::shared_ptr<RestoreBatch>& batch) {
        if (!batch || cancelled.load(std::memory_order_acquire)
            || restoreGeneration.load(std::memory_order_acquire) != batch->generation)
            return;

        bool anyPending = false;
        for (auto& pending : batch->decks) {
            if (pending.complete) continue;
            anyPending = true;
            if (owner.sessionDeckIsLoading(pending.deck)) continue;

            if (owner.sessionDeckMatchesFile(pending.deck, pending.file)) {
                owner.applySessionDeckState(pending.deck, pending.state, pending.durationSeconds);
                pending.complete = true;
                ++batch->restoredDecks;
            } else {
                pending.complete = true;
                ++batch->failedLoads;
            }
        }

        if (anyPending && batch->pollsRemaining-- > 0) {
            const auto weak = std::weak_ptr<int>(lifetime);
            juce::Timer::callAfterDelay(50, [this, weak, batch] {
                if (!weak.expired()) pollSessionRestore(batch);
            });
            return;
        }

        if (anyPending) {
            for (auto& pending : batch->decks) {
                if (!pending.complete) {
                    pending.complete = true;
                    ++batch->failedLoads;
                }
            }
        }
        finishSessionRestore(*batch);
    }

    void finishSessionRestore(const RestoreBatch& batch) {
        if (cancelled.load(std::memory_order_acquire)
            || restoreGeneration.load(std::memory_order_acquire) != batch.generation)
            return;
        if (!panel) return;

        panel->setBusy(false);
        juce::String message = batch.usedBackup
            ? uiText("Recovered verified backup; ", "Odzyskano zweryfikowaną kopię; ")
            : uiText("Session loaded; ", "Sesja wczytana; ");
        message << uiText("restored paused decks: ", "przywrócone zatrzymane decki: ")
                << batch.restoredDecks;
        if (batch.missingFiles > 0)
            message << uiText(" | missing files: ", " | brakujące pliki: ") << batch.missingFiles;
        if (batch.unreadableFiles > 0)
            message << uiText(" | unreadable: ", " | nieczytelne: ") << batch.unreadableFiles;
        if (batch.busyDecks > 0)
            message << uiText(" | busy decks skipped: ", " | pominięte zajęte decki: ") << batch.busyDecks;
        if (batch.failedLoads > 0)
            message << uiText(" | load failures/timeouts: ", " | błędy/timeout wczytania: ") << batch.failedLoads;
        if (batch.skippedEmpty > 0)
            message << uiText(" | empty session slots unchanged: ", " | puste sloty sesji bez zmian: ")
                    << batch.skippedEmpty;
        message << uiText(" | transport stays paused.", " | transport pozostaje zatrzymany.");
        panel->setMessage(message);
    }

    MainComponent& owner;
    juce::TextButton button;
    broke::library::LibraryDatabase database;
    juce::ThreadPool searchWorkers{1};
    juce::ThreadPool writeWorkers{1};
    juce::ThreadPool sessionWorkers{1};
    std::atomic<bool> cancelled{false};
    std::atomic<std::uint64_t> queryGeneration{0};
    std::atomic<std::uint64_t> restoreGeneration{0};
    std::shared_ptr<int> lifetime;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::FileChooser> sessionChooser;
    juce::Component::SafePointer<juce::DialogWindow> dialog;
    juce::Component::SafePointer<LibraryPanel> panel;
};