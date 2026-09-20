// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "LibraryPanel.h"
#include "MainComponent.h"

#include <JuceHeader.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <utility>

// Native, local-only M4 library workflow. Database/file work is performed by
// bounded worker pools and never from MainComponent::getNextAudioBlock().
class LibraryWorkflow final {
public:
    explicit LibraryWorkflow(MainComponent& target) : owner(target) {
        lifetime = std::make_shared<int>(0);
        button.setButtonText(uiText("Library", "Biblioteka"));
        button.setTooltip(uiText("Open the local BrokeDJ music library. No cloud account is used.",
                                 "Otwórz lokalną bibliotekę muzyki BrokeDJ. Konto w chmurze nie jest używane."));
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
        lifetime.reset();
        chooser.reset();
        if (dialog) delete dialog.getComponent();
        searchWorkers.removeAllJobs(true, -1);
        writeWorkers.removeAllJobs(true, -1);
        database.close();
    }

    LibraryWorkflow(const LibraryWorkflow&) = delete;
    LibraryWorkflow& operator=(const LibraryWorkflow&) = delete;

    void setButtonBounds(juce::Rectangle<int> bounds) { button.setBounds(bounds); }

private:
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
        owner.loadFileIntoDeck(deck, file);
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

    MainComponent& owner;
    juce::TextButton button;
    broke::library::LibraryDatabase database;
    juce::ThreadPool searchWorkers{1};
    juce::ThreadPool writeWorkers{1};
    std::atomic<bool> cancelled{false};
    std::atomic<std::uint64_t> queryGeneration{0};
    std::shared_ptr<int> lifetime;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Component::SafePointer<juce::DialogWindow> dialog;
    juce::Component::SafePointer<LibraryPanel> panel;
};
