// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "LibraryDatabase.h"

#include <JuceHeader.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

class LibraryPanel final : public juce::Component, private juce::ListBoxModel {
public:
    LibraryPanel() : list("BrokeDJ library", this) {
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
        saveSessionButton.setButtonText(uiText("Save session", "Zapisz sesję"));
        loadSessionButton.setButtonText(uiText("Load session", "Wczytaj sesję"));
        recoverSessionButton.setButtonText(uiText("Recover", "Odzyskaj"));

        importButton.onClick = [this] { if (onImportFiles) onImportFiles(); };
        loadButton.onClick = [this] { loadSelected(); };
        relocateButton.onClick = [this] {
            if (const auto* selected = selectedTrack(); selected != nullptr && onRelocateTrack)
                onRelocateTrack(*selected);
        };
        saveSessionButton.onClick = [this] { if (onSaveSession) onSaveSession(); };
        loadSessionButton.onClick = [this] { if (onLoadSession) onLoadSession(); };
        recoverSessionButton.onClick = [this] { if (onRecoverSession) onRecoverSession(); };

        saveSessionButton.setTooltip(uiText(
            "Save four deck/mixer controls and local track paths to a versioned BrokeDJ session file.",
            "Zapisz ustawienia czterech decków/miksera i lokalne ścieżki utworów do wersjonowanego pliku sesji BrokeDJ."));
        loadSessionButton.setTooltip(uiText(
            "Load a verified BrokeDJ session. Restored decks stay paused until you press Play.",
            "Wczytaj zweryfikowaną sesję BrokeDJ. Przywrócone decki pozostają zatrzymane, dopóki nie naciśniesz Play."));
        recoverSessionButton.setTooltip(uiText(
            "Explicitly try the verified .bak snapshot when the selected primary session cannot be loaded.",
            "Jawnie spróbuj zweryfikowanej kopii .bak, gdy wybranej głównej sesji nie można wczytać."));

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
        for (auto* child : std::array<juce::Component*, 11>{
                 &heading, &message, &search, &importButton, &loadButton,
                 &relocateButton, &saveSessionButton, &loadSessionButton,
                 &recoverSessionButton, &targetDeck, &list})
            addAndMakeVisible(child);
        updateButtons();
    }

    std::function<void(const juce::String&)> onSearchChanged;
    std::function<void()> onImportFiles;
    std::function<void(const broke::library::TrackRecord&, std::size_t)> onLoadTrack;
    std::function<void(const broke::library::TrackRecord&)> onRelocateTrack;
    std::function<void()> onSaveSession;
    std::function<void()> onLoadSession;
    std::function<void()> onRecoverSession;

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
        heading.setBounds(header.removeFromLeft(230));
        message.setBounds(header);
        area.removeFromTop(8);

        auto searchRow = area.removeFromTop(34);
        importButton.setBounds(searchRow.removeFromRight(116).reduced(4, 0));
        search.setBounds(searchRow.reduced(0, 1));
        area.removeFromTop(8);

        auto sessionActions = area.removeFromBottom(38);
        recoverSessionButton.setBounds(sessionActions.removeFromRight(108).reduced(4, 3));
        loadSessionButton.setBounds(sessionActions.removeFromRight(128).reduced(4, 3));
        saveSessionButton.setBounds(sessionActions.removeFromRight(128).reduced(4, 3));
        area.removeFromBottom(4);

        auto trackActions = area.removeFromBottom(38);
        targetDeck.setBounds(trackActions.removeFromLeft(110).reduced(2, 3));
        loadButton.setBounds(trackActions.removeFromLeft(140).reduced(4, 3));
        relocateButton.setBounds(trackActions.removeFromLeft(150).reduced(4, 3));
        list.setBounds(area);
    }

    void paint(juce::Graphics& g) override { g.fillAll(background); }

private:
    static juce::String uiText(const char* english, const char* polish) {
        static const bool usePolish = juce::SystemStats::getUserLanguage().startsWithIgnoreCase("pl");
        return juce::String::fromUTF8(usePolish ? polish : english);
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

    void selectedRowsChanged(int) override { updateButtons(); }

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

    void updateButtons() {
        const auto* selected = selectedTrack();
        loadButton.setEnabled(!busy && selected != nullptr && !selected->missing);
        relocateButton.setEnabled(!busy && selected != nullptr);
        saveSessionButton.setEnabled(!busy);
        loadSessionButton.setEnabled(!busy);
        recoverSessionButton.setEnabled(!busy);
    }

    inline static const juce::Colour background{0xff080e1a};
    inline static const juce::Colour panel{0xff111d30};
    inline static const juce::Colour blue{0xff3d9bff};
    inline static const juce::Colour pale{0xffdcecff};
    inline static const juce::Colour muted{0xff8199b8};

    juce::Label heading, message;
    juce::TextEditor search;
    juce::TextButton importButton, loadButton, relocateButton;
    juce::TextButton saveSessionButton, loadSessionButton, recoverSessionButton;
    juce::ComboBox targetDeck;
    juce::ListBox list;
    std::vector<broke::library::TrackRecord> rows;
    bool busy = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LibraryPanel)
};