// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "LibraryDatabase.h"

#include <JuceHeader.h>
#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class LibraryPanel final : public juce::Component, private juce::ListBoxModel {
public:
    LibraryPanel() : list("BrokeDJ library", this) {
        organizerLifetime = std::make_shared<int>(0);

        heading.setText(uiText("LOCAL LIBRARY", "LOKALNA BIBLIOTEKA"), juce::dontSendNotification);
        heading.setColour(juce::Label::textColourId, pale);
        heading.setFont(juce::Font(juce::FontOptions(18.0f).withStyle("Bold")));

        message.setText(uiText("Search or import local music. Nothing is uploaded.",
                               "Szukaj lub importuj lokalną muzykę. Nic nie jest wysyłane."),
                        juce::dontSendNotification);
        message.setColour(juce::Label::textColourId, muted);
        message.setFont(juce::Font(juce::FontOptions(12.0f)));

        playlistFilter.addItem(uiText("All tracks", "Wszystkie utwory"), 1);
        playlistFilter.setSelectedId(1, juce::dontSendNotification);
        playlistFilter.setTooltip(uiText(
            "Browse one local playlist without loading the whole collection into memory.",
            "Przeglądaj jedną lokalną playlistę bez wczytywania całej kolekcji do pamięci."));
        playlistFilter.onChange = [this] { runCurrentSearch(); };

        search.setTextToShowWhenEmpty(uiText("Search title, artist, album, tag or path…",
                                             "Szukaj tytułu, artysty, albumu, tagu lub ścieżki…"), muted);
        search.setColour(juce::TextEditor::backgroundColourId, panel);
        search.setColour(juce::TextEditor::textColourId, pale);
        search.setColour(juce::TextEditor::outlineColourId, blue.withAlpha(0.35f));
        search.onTextChange = [this] { runCurrentSearch(); };

        importButton.setButtonText(uiText("Import files", "Importuj pliki"));
        loadButton.setButtonText(uiText("Load selected", "Wczytaj wybrany"));
        relocateButton.setButtonText(uiText("Relocate", "Wskaż nowy plik"));
        importButton.onClick = [this] { if (onImportFiles) onImportFiles(); };
        loadButton.onClick = [this] { loadSelected(); };
        relocateButton.onClick = [this] {
            if (const auto* selected = selectedTrack(); selected != nullptr && onRelocateTrack)
                onRelocateTrack(*selected);
        };

        tagEditor.setTextToShowWhenEmpty(uiText("Tag (e.g. Techno)", "Tag (np. Techno)"), muted);
        tagEditor.setColour(juce::TextEditor::backgroundColourId, panel);
        tagEditor.setColour(juce::TextEditor::textColourId, pale);
        tagEditor.setColour(juce::TextEditor::outlineColourId, blue.withAlpha(0.35f));
        tagEditor.setInputRestrictions(64);
        tagEditor.onTextChange = [this] { updateButtons(); };
        addTagButton.setButtonText(uiText("Add tag", "Dodaj tag"));
        removeTagButton.setButtonText(uiText("Remove tag", "Usuń tag"));
        addTagButton.onClick = [this] { editTag(true); };
        removeTagButton.onClick = [this] { editTag(false); };

        playlistEditor.setTextToShowWhenEmpty(uiText("Playlist name", "Nazwa playlisty"), muted);
        playlistEditor.setColour(juce::TextEditor::backgroundColourId, panel);
        playlistEditor.setColour(juce::TextEditor::textColourId, pale);
        playlistEditor.setColour(juce::TextEditor::outlineColourId, blue.withAlpha(0.35f));
        playlistEditor.setInputRestrictions(96);
        playlistEditor.onTextChange = [this] { updateButtons(); };
        addPlaylistButton.setButtonText(uiText("Add / create", "Dodaj / utwórz"));
        removePlaylistButton.setButtonText(uiText("Remove from", "Usuń z playlisty"));
        addPlaylistButton.onClick = [this] { editPlaylist(true); };
        removePlaylistButton.onClick = [this] { editPlaylist(false); };

        selectionMeta.setText(uiText("Select a track to edit tags/playlists.",
                                     "Wybierz utwór, aby edytować tagi/playlisty."),
                              juce::dontSendNotification);
        selectionMeta.setColour(juce::Label::textColourId, muted);
        selectionMeta.setFont(juce::Font(juce::FontOptions(10.5f)));
        selectionMeta.setJustificationType(juce::Justification::centredLeft);

        for (int i = 0; i < 4; ++i)
            targetDeck.addItem(uiText("Deck ", "Deck ") + juce::String::charToString(
                static_cast<juce::juce_wchar>('A' + i)), i + 1);
        targetDeck.setSelectedId(1, juce::dontSendNotification);
        targetDeck.setTooltip(uiText("Target deck for loading the selected library track.",
                                     "Docelowy deck dla wybranego utworu z biblioteki."));

        list.setRowHeight(34);
        list.setColour(juce::ListBox::backgroundColourId, background);
        list.setColour(juce::ListBox::outlineColourId, blue.withAlpha(0.22f));
        list.setOutlineThickness(1);
        for (auto* child : std::array<juce::Component*, 16>{
                 &heading, &message, &playlistFilter, &search, &importButton, &loadButton,
                 &relocateButton, &tagEditor, &addTagButton, &removeTagButton,
                 &playlistEditor, &addPlaylistButton, &removePlaylistButton,
                 &selectionMeta, &targetDeck, &list})
            addAndMakeVisible(child);

        const auto databaseFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                      .getChildFile("BrokeDJ")
                                      .getChildFile("library.sqlite3");
        organizerDatabasePath = filesystemPath(databaseFile);
        std::string error;
        organizerAvailable = organizerDb.open(organizerDatabasePath, &error);
        if (!organizerAvailable) {
            selectionMeta.setText(uiText("Tag/playlist editing unavailable; library playback still works.",
                                         "Edycja tagów/playlist niedostępna; odtwarzanie biblioteki nadal działa."),
                                  juce::dontSendNotification);
        } else {
            refreshPlaylistCatalog();
        }
        updateButtons();
    }

    ~LibraryPanel() override {
        organizerCancelled.store(true, std::memory_order_release);
        organizerLifetime.reset();
        metadataWorkers.removeAllJobs(true, -1);
        browseWorkers.removeAllJobs(true, -1);
        organizerWorkers.removeAllJobs(true, -1);
        organizerDb.close();
    }

    std::function<void(const juce::String&)> onSearchChanged;
    std::function<void()> onImportFiles;
    std::function<void(const broke::library::TrackRecord&, std::size_t)> onLoadTrack;
    std::function<void(const broke::library::TrackRecord&)> onRelocateTrack;

    void setResults(std::vector<broke::library::TrackRecord> tracks) {
        applyResults(std::move(tracks), {});
    }

    void setBusy(bool nextBusy) {
        busy = nextBusy;
        search.setEnabled(!busy);
        playlistFilter.setEnabled(!busy && organizerAvailable);
        importButton.setEnabled(!busy);
        if (busy) setMessage(uiText("Working…", "Przetwarzanie…"));
        updateButtons();
    }

    void setMessage(const juce::String& value) {
        message.setText(value, juce::dontSendNotification);
        message.setTooltip(value);
    }

    [[nodiscard]] juce::String query() const { return search.getText(); }

    void resized() override {
        auto area = getLocalBounds().reduced(14);
        auto header = area.removeFromTop(34);
        heading.setBounds(header.removeFromLeft(std::min(230, header.getWidth() / 2)));
        message.setBounds(header);
        area.removeFromTop(8);

        auto searchRow = area.removeFromTop(34);
        const int importWidth = std::clamp(searchRow.getWidth() / 5, 96, 124);
        const int playlistWidth = std::clamp(searchRow.getWidth() / 4, 150, 220);
        importButton.setBounds(searchRow.removeFromRight(importWidth).reduced(4, 0));
        playlistFilter.setBounds(searchRow.removeFromLeft(playlistWidth).reduced(0, 1));
        searchRow.removeFromLeft(6);
        search.setBounds(searchRow.reduced(0, 1));
        area.removeFromTop(8);

        auto bottom = area.removeFromBottom(116);
        auto tagRow = bottom.removeFromTop(36);
        bottom.removeFromTop(3);
        auto playlistRow = bottom.removeFromTop(36);
        bottom.removeFromTop(3);
        auto actions = bottom.removeFromTop(38);

        const auto boundedEditorWidth = [](int width) {
            return std::clamp(width / 3, 120, 240);
        };
        const auto boundedButtonWidth = [](int width) {
            return std::clamp(width / 5, 88, 132);
        };

        int editorWidth = boundedEditorWidth(tagRow.getWidth());
        int buttonWidth = boundedButtonWidth(tagRow.getWidth());
        tagEditor.setBounds(tagRow.removeFromLeft(editorWidth).reduced(2, 3));
        addTagButton.setBounds(tagRow.removeFromLeft(buttonWidth).reduced(3));
        removeTagButton.setBounds(tagRow.removeFromLeft(buttonWidth).reduced(3));
        selectionMeta.setBounds(tagRow.reduced(5, 0));

        editorWidth = boundedEditorWidth(playlistRow.getWidth());
        buttonWidth = boundedButtonWidth(playlistRow.getWidth());
        playlistEditor.setBounds(playlistRow.removeFromLeft(editorWidth).reduced(2, 3));
        addPlaylistButton.setBounds(playlistRow.removeFromLeft(buttonWidth + 22).reduced(3));
        removePlaylistButton.setBounds(playlistRow.removeFromLeft(buttonWidth + 30).reduced(3));

        targetDeck.setBounds(actions.removeFromLeft(std::clamp(actions.getWidth() / 5, 94, 116)).reduced(2, 3));
        loadButton.setBounds(actions.removeFromLeft(std::clamp(actions.getWidth() / 4, 116, 150)).reduced(4, 3));
        relocateButton.setBounds(actions.removeFromLeft(std::clamp(actions.getWidth() / 3, 126, 160)).reduced(4, 3));
        list.setBounds(area);
    }

    void paint(juce::Graphics& g) override { g.fillAll(background); }

private:
    struct PlaylistSummary final {
        std::int64_t id = -1;
        std::string name;
        std::int64_t trackCount = 0;
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

    static std::string pathUtf8(const std::filesystem::path& path) {
        const auto value = path.lexically_normal().generic_u8string();
        return std::string(reinterpret_cast<const char*>(value.data()), value.size());
    }

    static std::string columnText(sqlite3_stmt* statement, int column) {
        const auto* value = sqlite3_column_text(statement, column);
        if (value == nullptr) return {};
        const int bytes = sqlite3_column_bytes(statement, column);
        return std::string(reinterpret_cast<const char*>(value), static_cast<std::size_t>(std::max(0, bytes)));
    }

    static broke::library::TrackRecord readTrack(sqlite3_stmt* statement) {
        broke::library::TrackRecord track;
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

    static sqlite3* openReadOnly(const std::filesystem::path& databasePath) {
        if (databasePath.empty()) return nullptr;
        sqlite3* handle = nullptr;
        const auto path = pathUtf8(databasePath);
        if (sqlite3_open_v2(path.c_str(), &handle,
                            SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK
            || handle == nullptr) {
            if (handle != nullptr) sqlite3_close_v2(handle);
            return nullptr;
        }
        sqlite3_busy_timeout(handle, 2500);
        return handle;
    }

    static juce::String displayTitle(const broke::library::TrackRecord& track) {
        juce::String title = juce::String::fromUTF8(track.title.c_str());
        if (title.isEmpty())
            title = juce::File(juce::String::fromUTF8(track.path.c_str())).getFileNameWithoutExtension();
        const auto artist = juce::String::fromUTF8(track.artist.c_str());
        if (artist.isNotEmpty()) title = artist + " — " + title;
        if (track.missing) title += uiText("  [MISSING]", "  [BRAK PLIKU]");
        return title;
    }

    static std::optional<std::int64_t> findPlaylistId(const std::filesystem::path& databasePath,
                                                       std::string_view name) {
        if (name.empty()) return std::nullopt;
        sqlite3* handle = openReadOnly(databasePath);
        if (handle == nullptr) return std::nullopt;
        sqlite3_stmt* statement = nullptr;
        const char* sql = "SELECT id FROM playlists WHERE name=?1 COLLATE NOCASE LIMIT 1;";
        std::optional<std::int64_t> result;
        if (sqlite3_prepare_v2(handle, sql, -1, &statement, nullptr) == SQLITE_OK
            && statement != nullptr
            && sqlite3_bind_text(statement, 1, name.data(), static_cast<int>(name.size()),
                                 SQLITE_TRANSIENT) == SQLITE_OK
            && sqlite3_step(statement) == SQLITE_ROW) {
            result = sqlite3_column_int64(statement, 0);
        }
        if (statement != nullptr) sqlite3_finalize(statement);
        sqlite3_close_v2(handle);
        return result;
    }

    static std::vector<PlaylistSummary> readPlaylistCatalog(const std::filesystem::path& databasePath) {
        std::vector<PlaylistSummary> result;
        sqlite3* handle = openReadOnly(databasePath);
        if (handle == nullptr) return result;
        sqlite3_stmt* statement = nullptr;
        constexpr const char* sql =
            "SELECT p.id,p.name,COUNT(pi.track_id) FROM playlists p "
            "LEFT JOIN playlist_items pi ON pi.playlist_id=p.id "
            "GROUP BY p.id,p.name ORDER BY p.name COLLATE NOCASE,p.id LIMIT 256;";
        if (sqlite3_prepare_v2(handle, sql, -1, &statement, nullptr) == SQLITE_OK
            && statement != nullptr) {
            while (sqlite3_step(statement) == SQLITE_ROW) {
                result.push_back(PlaylistSummary{sqlite3_column_int64(statement, 0),
                                                 columnText(statement, 1),
                                                 sqlite3_column_int64(statement, 2)});
            }
        }
        if (statement != nullptr) sqlite3_finalize(statement);
        sqlite3_close_v2(handle);
        return result;
    }

    static std::vector<std::string> readPlaylistNamesForTrack(
        const std::filesystem::path& databasePath, std::int64_t trackId) {
        std::vector<std::string> result;
        sqlite3* handle = openReadOnly(databasePath);
        if (handle == nullptr) return result;
        sqlite3_stmt* statement = nullptr;
        constexpr const char* sql =
            "SELECT p.name FROM playlists p JOIN playlist_items pi ON pi.playlist_id=p.id "
            "WHERE pi.track_id=?1 ORDER BY p.name COLLATE NOCASE,p.id LIMIT 64;";
        if (sqlite3_prepare_v2(handle, sql, -1, &statement, nullptr) == SQLITE_OK
            && statement != nullptr
            && sqlite3_bind_int64(statement, 1, trackId) == SQLITE_OK) {
            while (sqlite3_step(statement) == SQLITE_ROW)
                result.push_back(columnText(statement, 0));
        }
        if (statement != nullptr) sqlite3_finalize(statement);
        sqlite3_close_v2(handle);
        return result;
    }

    static std::vector<broke::library::TrackRecord> readPlaylistTracks(
        const std::filesystem::path& databasePath, std::int64_t playlistId,
        std::string_view query, std::size_t limit) {
        std::vector<broke::library::TrackRecord> result;
        sqlite3* handle = openReadOnly(databasePath);
        if (handle == nullptr) return result;
        limit = std::clamp<std::size_t>(limit, 1, 500);

        const std::string columns =
            "t.id,t.path,t.file_size,t.modified_ns,t.title,t.artist,t.album,"
            "t.duration_seconds,t.bpm,t.musical_key,t.content_hash,t.missing";
        std::string sql = "SELECT DISTINCT " + columns
            + " FROM playlist_items pi JOIN tracks t ON t.id=pi.track_id WHERE pi.playlist_id=?1 ";
        const bool duplicateOnly = query == broke::library::LibraryDatabase::duplicateSearchDirective;
        const bool missingOnly = query == broke::library::LibraryDatabase::missingSearchDirective;
        if (duplicateOnly) {
            sql += "AND t.missing=0 AND t.content_hash<>'' "
                   "AND t.content_hash IN(SELECT content_hash FROM tracks WHERE missing=0 "
                   "AND content_hash<>'' GROUP BY content_hash HAVING COUNT(*)>1) ";
        } else if (missingOnly) {
            sql += "AND t.missing=1 ";
        } else {
            sql += "AND (?2='' OR instr(lower(t.title),lower(?2))>0 "
                   "OR instr(lower(t.artist),lower(?2))>0 "
                   "OR instr(lower(t.album),lower(?2))>0 "
                   "OR instr(lower(t.path),lower(?2))>0 "
                   "OR EXISTS(SELECT 1 FROM track_tags tt JOIN tags g ON g.id=tt.tag_id "
                   "WHERE tt.track_id=t.id AND instr(lower(g.name),lower(?2))>0)) ";
        }
        sql += "ORDER BY pi.position,pi.rowid LIMIT ?3;";

        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(handle, sql.c_str(), -1, &statement, nullptr) == SQLITE_OK
            && statement != nullptr
            && sqlite3_bind_int64(statement, 1, playlistId) == SQLITE_OK) {
            bool bound = true;
            if (!duplicateOnly && !missingOnly) {
                bound = sqlite3_bind_text(statement, 2, query.data(), static_cast<int>(query.size()),
                                           SQLITE_TRANSIENT) == SQLITE_OK;
            } else {
                bound = sqlite3_bind_null(statement, 2) == SQLITE_OK;
            }
            bound = bound && sqlite3_bind_int64(statement, 3,
                static_cast<sqlite3_int64>(limit)) == SQLITE_OK;
            if (bound) {
                while (sqlite3_step(statement) == SQLITE_ROW)
                    result.push_back(readTrack(statement));
            }
        }
        if (statement != nullptr) sqlite3_finalize(statement);
        sqlite3_close_v2(handle);
        return result;
    }

    int getNumRows() override { return static_cast<int>(rows.size()); }

    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
                          bool rowIsSelected) override {
        if (rowNumber < 0 || rowNumber >= static_cast<int>(rows.size())) return;
        const auto& track = rows[static_cast<std::size_t>(rowNumber)];
        if (rowIsSelected) g.fillAll(blue.withAlpha(0.24f));
        else if ((rowNumber & 1) != 0) g.fillAll(panel.withAlpha(0.55f));

        auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(8, 2);
        g.setColour(track.missing ? muted : pale);
        g.setFont(13.0f);
        g.drawFittedText(displayTitle(track), bounds.removeFromTop(18),
                         juce::Justification::centredLeft, 1);
        g.setColour(muted);
        g.setFont(10.5f);
        g.drawFittedText(juce::String::fromUTF8(track.path.c_str()), bounds,
                         juce::Justification::centredLeft, 1);
    }

    void selectedRowsChanged(int) override {
        refreshSelectionMetadata();
        updateButtons();
    }

    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override {
        if (row >= 0 && row < static_cast<int>(rows.size())) {
            list.selectRow(row);
            loadSelected();
        }
    }

    [[nodiscard]] const broke::library::TrackRecord* selectedTrack() const noexcept {
        const int selected = list.getSelectedRow();
        if (selected < 0 || selected >= static_cast<int>(rows.size())) return nullptr;
        return &rows[static_cast<std::size_t>(selected)];
    }

    [[nodiscard]] std::optional<std::int64_t> selectedPlaylistId() const noexcept {
        const int selected = playlistFilter.getSelectedId();
        if (selected < 2) return std::nullopt;
        const auto index = static_cast<std::size_t>(selected - 2);
        if (index >= playlists.size()) return std::nullopt;
        return playlists[index].id;
    }

    void loadSelected() {
        if (busy) return;
        const auto* selected = selectedTrack();
        const int target = targetDeck.getSelectedId();
        if (selected != nullptr && target >= 1 && target <= 4 && onLoadTrack)
            onLoadTrack(*selected, static_cast<std::size_t>(target - 1));
    }

    void runCurrentSearch() {
        if (busy) return;
        if (const auto playlistId = selectedPlaylistId()) {
            queuePlaylistBrowse(*playlistId, search.getText());
        } else if (onSearchChanged) {
            onSearchChanged(search.getText());
        }
    }

    void queuePlaylistBrowse(std::int64_t playlistId, const juce::String& queryText) {
        if (!organizerAvailable || organizerCancelled.load(std::memory_order_acquire)) return;
        const auto generation = browseGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
        browseWorkers.removeAllJobs(false, 0);
        const auto databasePath = organizerDatabasePath;
        const auto queryUtf8 = queryText.toStdString();
        const auto weak = std::weak_ptr<int>(organizerLifetime);
        setBusy(true);
        browseWorkers.addJob([this, weak, generation, playlistId, databasePath, queryUtf8] {
            if (weak.expired() || organizerCancelled.load(std::memory_order_acquire)) return;
            auto results = readPlaylistTracks(databasePath, playlistId, queryUtf8, 500);
            juce::MessageManager::callAsync(
                [this, weak, generation, playlistId, results = std::move(results)]() mutable {
                    if (weak.expired() || organizerCancelled.load(std::memory_order_acquire)
                        || browseGeneration.load(std::memory_order_acquire) != generation)
                        return;
                    const auto current = selectedPlaylistId();
                    if (!current || *current != playlistId) return;
                    juce::String context = uiText("Playlist tracks: ", "Utwory playlisty: ")
                        + juce::String(static_cast<int>(results.size()));
                    applyResults(std::move(results), context);
                });
        });
    }

    void applyResults(std::vector<broke::library::TrackRecord> tracks, const juce::String& context) {
        const auto* before = selectedTrack();
        const std::int64_t selectedId = before != nullptr ? before->id : -1;
        rows = std::move(tracks);
        list.updateContent();
        int selectedRow = -1;
        if (selectedId >= 0) {
            for (std::size_t i = 0; i < rows.size(); ++i) {
                if (rows[i].id == selectedId) {
                    selectedRow = static_cast<int>(i);
                    break;
                }
            }
        }
        if (selectedRow >= 0) list.selectRow(selectedRow);
        else list.deselectAllRows();
        setBusy(false);
        if (context.isNotEmpty()) {
            setMessage(context);
        } else {
            setMessage(rows.empty()
                ? uiText("No matching tracks.", "Brak pasujących utworów.")
                : uiText("Tracks: ", "Utwory: ") + juce::String(static_cast<int>(rows.size())));
        }
        refreshSelectionMetadata();
        updateButtons();
    }

    void refreshPlaylistCatalog() {
        if (!organizerAvailable || organizerCancelled.load(std::memory_order_acquire)) return;
        const auto generation = playlistGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
        const auto databasePath = organizerDatabasePath;
        const auto selectedBefore = selectedPlaylistId();
        const auto weak = std::weak_ptr<int>(organizerLifetime);
        browseWorkers.addJob([this, weak, generation, databasePath, selectedBefore] {
            if (weak.expired() || organizerCancelled.load(std::memory_order_acquire)) return;
            auto catalog = readPlaylistCatalog(databasePath);
            juce::MessageManager::callAsync(
                [this, weak, generation, selectedBefore, catalog = std::move(catalog)]() mutable {
                    if (weak.expired() || organizerCancelled.load(std::memory_order_acquire)
                        || playlistGeneration.load(std::memory_order_acquire) != generation)
                        return;
                    playlists = std::move(catalog);
                    playlistFilter.clear(juce::dontSendNotification);
                    playlistFilter.addItem(uiText("All tracks", "Wszystkie utwory"), 1);
                    int selectedUiId = 1;
                    for (std::size_t i = 0; i < playlists.size(); ++i) {
                        const int uiId = static_cast<int>(i) + 2;
                        juce::String label = juce::String::fromUTF8(playlists[i].name.c_str());
                        label += " (" + juce::String(playlists[i].trackCount) + ")";
                        playlistFilter.addItem(label, uiId);
                        if (selectedBefore && playlists[i].id == *selectedBefore) selectedUiId = uiId;
                    }
                    playlistFilter.setSelectedId(selectedUiId, juce::dontSendNotification);
                    playlistFilter.setEnabled(!busy && organizerAvailable);
                });
        });
    }

    void refreshSelectionMetadata() {
        const auto* selected = selectedTrack();
        const auto generation = metadataGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
        metadataWorkers.removeAllJobs(false, 0);
        if (selected == nullptr) {
            selectionMeta.setText(organizerAvailable
                ? uiText("Select a track to edit tags/playlists.",
                         "Wybierz utwór, aby edytować tagi/playlisty.")
                : uiText("Tag/playlist editing unavailable.", "Edycja tagów/playlist niedostępna."),
                juce::dontSendNotification);
            return;
        }
        if (!organizerAvailable) return;

        const auto trackId = selected->id;
        const auto databasePath = organizerDatabasePath;
        const auto weak = std::weak_ptr<int>(organizerLifetime);
        selectionMeta.setText(uiText("Loading tags/playlists…", "Wczytywanie tagów/playlist…"),
                              juce::dontSendNotification);
        metadataWorkers.addJob([this, weak, generation, trackId, databasePath] {
            if (weak.expired() || organizerCancelled.load(std::memory_order_acquire)) return;
            std::string error;
            const auto tags = organizerDb.tagsForTrack(trackId, &error);
            const auto memberships = readPlaylistNamesForTrack(databasePath, trackId);
            juce::String summary;
            if (!error.empty()) {
                summary = uiText("Tags unavailable", "Tagi niedostępne");
            } else if (tags.empty()) {
                summary = uiText("Tags: none", "Tagi: brak");
            } else {
                summary = uiText("Tags: ", "Tagi: ");
                for (std::size_t i = 0; i < tags.size(); ++i) {
                    if (i != 0) summary += ", ";
                    summary += juce::String::fromUTF8(tags[i].c_str());
                }
            }
            summary += uiText("  |  Playlists: ", "  |  Playlisty: ");
            if (memberships.empty()) {
                summary += uiText("none", "brak");
            } else {
                for (std::size_t i = 0; i < memberships.size(); ++i) {
                    if (i != 0) summary += ", ";
                    summary += juce::String::fromUTF8(memberships[i].c_str());
                }
            }
            juce::MessageManager::callAsync([this, weak, generation, trackId, summary] {
                if (weak.expired() || organizerCancelled.load(std::memory_order_acquire)
                    || metadataGeneration.load(std::memory_order_acquire) != generation)
                    return;
                const auto* current = selectedTrack();
                if (current != nullptr && current->id == trackId)
                    selectionMeta.setText(summary, juce::dontSendNotification);
            });
        });
    }

    void editTag(bool add) {
        const auto* selected = selectedTrack();
        auto text = tagEditor.getText().trim();
        if (selected == nullptr || text.isEmpty() || !organizerAvailable || editBusy) return;
        if (text.length() > 64) text = text.substring(0, 64);
        const auto trackId = selected->id;
        const auto tag = text.toStdString();
        const auto weak = std::weak_ptr<int>(organizerLifetime);
        setEditBusy(true);
        organizerWorkers.addJob([this, weak, trackId, tag, add] {
            if (weak.expired() || organizerCancelled.load(std::memory_order_acquire)) return;
            std::string error;
            const bool ok = add ? organizerDb.addTag(trackId, tag, &error)
                                : organizerDb.removeTag(trackId, tag, &error);
            juce::MessageManager::callAsync([this, weak, ok, add] {
                if (weak.expired() || organizerCancelled.load(std::memory_order_acquire)) return;
                setEditBusy(false);
                setMessage(ok
                    ? (add ? uiText("Tag added.", "Dodano tag.")
                           : uiText("Tag removed.", "Usunięto tag."))
                    : uiText("Tag edit failed.", "Edycja tagu nie powiodła się."));
                if (ok) {
                    refreshSelectionMetadata();
                    runCurrentSearch();
                }
            });
        });
    }

    void editPlaylist(bool add) {
        const auto* selected = selectedTrack();
        auto text = playlistEditor.getText().trim();
        if (selected == nullptr || text.isEmpty() || !organizerAvailable || editBusy) return;
        if (text.length() > 96) text = text.substring(0, 96);
        const auto trackId = selected->id;
        const auto name = text.toStdString();
        const auto databasePath = organizerDatabasePath;
        const auto weak = std::weak_ptr<int>(organizerLifetime);
        setEditBusy(true);
        organizerWorkers.addJob([this, weak, trackId, name, databasePath, add] {
            if (weak.expired() || organizerCancelled.load(std::memory_order_acquire)) return;
            std::string error;
            bool found = true;
            bool ok = false;
            if (add) {
                const auto playlistId = organizerDb.createPlaylist(name, &error);
                ok = playlistId.has_value()
                    && organizerDb.addToPlaylist(*playlistId, trackId, &error);
            } else {
                const auto playlistId = findPlaylistId(databasePath, name);
                found = playlistId.has_value();
                ok = found && organizerDb.removeFromPlaylist(*playlistId, trackId, &error);
            }
            juce::MessageManager::callAsync([this, weak, ok, found, add] {
                if (weak.expired() || organizerCancelled.load(std::memory_order_acquire)) return;
                setEditBusy(false);
                if (!found) {
                    setMessage(uiText("Playlist not found; nothing changed.",
                                      "Nie znaleziono playlisty; nic nie zmieniono."));
                } else {
                    setMessage(ok
                        ? (add ? uiText("Track added to playlist.", "Dodano utwór do playlisty.")
                               : uiText("Track removed from playlist.", "Usunięto utwór z playlisty."))
                        : uiText("Playlist edit failed.", "Edycja playlisty nie powiodła się."));
                }
                if (ok) {
                    refreshSelectionMetadata();
                    refreshPlaylistCatalog();
                    runCurrentSearch();
                }
            });
        });
    }

    void setEditBusy(bool next) {
        editBusy = next;
        updateButtons();
    }

    void updateButtons() {
        const auto* selected = selectedTrack();
        loadButton.setEnabled(!busy && selected != nullptr && !selected->missing);
        relocateButton.setEnabled(!busy && selected != nullptr);
        const bool canEdit = organizerAvailable && !busy && !editBusy && selected != nullptr;
        const bool hasTag = tagEditor.getText().trim().isNotEmpty();
        const bool hasPlaylist = playlistEditor.getText().trim().isNotEmpty();
        tagEditor.setEnabled(organizerAvailable && !busy && !editBusy);
        playlistEditor.setEnabled(organizerAvailable && !busy && !editBusy);
        playlistFilter.setEnabled(organizerAvailable && !busy && !editBusy);
        addTagButton.setEnabled(canEdit && hasTag);
        removeTagButton.setEnabled(canEdit && hasTag);
        addPlaylistButton.setEnabled(canEdit && hasPlaylist);
        removePlaylistButton.setEnabled(canEdit && hasPlaylist);
    }

    inline static const juce::Colour background{0xff080e1a};
    inline static const juce::Colour panel{0xff111d30};
    inline static const juce::Colour blue{0xff3d9bff};
    inline static const juce::Colour pale{0xffdcecff};
    inline static const juce::Colour muted{0xff8199b8};

    juce::Label heading, message, selectionMeta;
    juce::TextEditor search, tagEditor, playlistEditor;
    juce::TextButton importButton, loadButton, relocateButton;
    juce::TextButton addTagButton, removeTagButton, addPlaylistButton, removePlaylistButton;
    juce::ComboBox playlistFilter, targetDeck;
    juce::ListBox list;
    std::vector<broke::library::TrackRecord> rows;
    std::vector<PlaylistSummary> playlists;
    bool busy = false;
    bool editBusy = false;

    broke::library::LibraryDatabase organizerDb;
    std::filesystem::path organizerDatabasePath;
    juce::ThreadPool metadataWorkers{1};
    juce::ThreadPool browseWorkers{1};
    juce::ThreadPool organizerWorkers{1};
    std::atomic<bool> organizerCancelled{false};
    std::atomic<std::uint64_t> metadataGeneration{0};
    std::atomic<std::uint64_t> browseGeneration{0};
    std::atomic<std::uint64_t> playlistGeneration{0};
    std::shared_ptr<int> organizerLifetime;
    bool organizerAvailable = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LibraryPanel)
};