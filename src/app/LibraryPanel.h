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

        search.setTextToShowWhenEmpty(uiText("Search title, artist, album, tag or path…",
                                             "Szukaj tytułu, artysty, albumu, tagu lub ścieżki…"), muted);
        search.setColour(juce::TextEditor::backgroundColourId, panel);
        search.setColour(juce::TextEditor::textColourId, pale);
        search.setColour(juce::TextEditor::outlineColourId, blue.withAlpha(0.35f));
        search.onTextChange = [this] { if (onSearchChanged) onSearchChanged(search.getText()); };

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
        for (auto* child : std::array<juce::Component*, 15>{
                 &heading, &message, &search, &importButton, &loadButton,
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
        if (!organizerAvailable)
            selectionMeta.setText(uiText("Tag/playlist editing unavailable; library playback still works.",
                                         "Edycja tagów/playlist niedostępna; odtwarzanie biblioteki nadal działa."),
                                  juce::dontSendNotification);
        updateButtons();
    }

    ~LibraryPanel() override {
        organizerCancelled.store(true, std::memory_order_release);
        organizerLifetime.reset();
        metadataWorkers.removeAllJobs(true, -1);
        organizerWorkers.removeAllJobs(true, -1);
        organizerDb.close();
    }

    std::function<void(const juce::String&)> onSearchChanged;
    std::function<void()> onImportFiles;
    std::function<void(const broke::library::TrackRecord&, std::size_t)> onLoadTrack;
    std::function<void(const broke::library::TrackRecord&)> onRelocateTrack;

    void setResults(std::vector<broke::library::TrackRecord> tracks) {
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
        setMessage(rows.empty()
            ? uiText("No matching tracks.", "Brak pasujących utworów.")
            : uiText("Tracks: ", "Utwory: ") + juce::String(static_cast<int>(rows.size())));
        refreshSelectionMetadata();
        updateButtons();
    }

    void setBusy(bool nextBusy) {
        busy = nextBusy;
        search.setEnabled(!busy);
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
        importButton.setBounds(searchRow.removeFromRight(importWidth).reduced(4, 0));
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
        if (databasePath.empty() || name.empty()) return std::nullopt;
        sqlite3* handle = nullptr;
        const auto path = pathUtf8(databasePath);
        if (sqlite3_open_v2(path.c_str(), &handle,
                            SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK
            || handle == nullptr) {
            if (handle != nullptr) sqlite3_close_v2(handle);
            return std::nullopt;
        }
        sqlite3_busy_timeout(handle, 2500);
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

    void loadSelected() {
        if (busy) return;
        const auto* selected = selectedTrack();
        const int target = targetDeck.getSelectedId();
        if (selected != nullptr && target >= 1 && target <= 4 && onLoadTrack)
            onLoadTrack(*selected, static_cast<std::size_t>(target - 1));
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
        const auto weak = std::weak_ptr<int>(organizerLifetime);
        selectionMeta.setText(uiText("Loading tags…", "Wczytywanie tagów…"), juce::dontSendNotification);
        metadataWorkers.addJob([this, weak, generation, trackId] {
            if (weak.expired() || organizerCancelled.load(std::memory_order_acquire)) return;
            std::string error;
            const auto tags = organizerDb.tagsForTrack(trackId, &error);
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
                    if (onSearchChanged) onSearchChanged(search.getText());
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
    juce::ComboBox targetDeck;
    juce::ListBox list;
    std::vector<broke::library::TrackRecord> rows;
    bool busy = false;
    bool editBusy = false;

    broke::library::LibraryDatabase organizerDb;
    std::filesystem::path organizerDatabasePath;
    juce::ThreadPool metadataWorkers{1};
    juce::ThreadPool organizerWorkers{1};
    std::atomic<bool> organizerCancelled{false};
    std::atomic<std::uint64_t> metadataGeneration{0};
    std::shared_ptr<int> organizerLifetime;
    bool organizerAvailable = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LibraryPanel)
};
