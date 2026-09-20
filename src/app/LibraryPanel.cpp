// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#include "LibraryPanel.h"

#include <algorithm>
#include <array>

namespace {
const juce::Colour background{0xff080e1a};
const juce::Colour panel{0xff111d30};
const juce::Colour blue{0xff3d9bff};
const juce::Colour pale{0xffdcecff};
const juce::Colour muted{0xff8199b8};

juce::String uiText(const char* english, const char* polish) {
    static const bool usePolish = juce::SystemStats::getUserLanguage().startsWithIgnoreCase("pl");
    return juce::String::fromUTF8(usePolish ? polish : english);
}

juce::String displayTitle(const broke::library::TrackRecord& track) {
    juce::String title = juce::String::fromUTF8(track.title.c_str());
    if (title.isEmpty())
        title = juce::File(juce::String::fromUTF8(track.path.c_str())).getFileNameWithoutExtension();
    const auto artist = juce::String::fromUTF8(track.artist.c_str());
    if (artist.isNotEmpty()) title = artist + " — " + title;
    if (track.missing) title += uiText("  [MISSING]", "  [BRAK PLIKU]");
    return title;
}
}

LibraryPanel::LibraryPanel() : list("BrokeDJ library", this) {
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
    search.onTextChange = [this] {
        if (onSearchChanged) onSearchChanged(search.getText());
    };

    importButton.setButtonText(uiText("Import files", "Importuj pliki"));
    loadButton.setButtonText(uiText("Load selected", "Wczytaj wybrany"));
    relocateButton.setButtonText(uiText("Relocate", "Wskaż nowy plik"));
    importButton.onClick = [this] { if (onImportFiles) onImportFiles(); };
    loadButton.onClick = [this] { loadSelected(); };
    relocateButton.onClick = [this] {
        if (const auto* selected = selectedTrack(); selected != nullptr && onRelocateTrack)
            onRelocateTrack(*selected);
    };

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

    for (auto* child : std::array<juce::Component*, 8>{
             &heading, &message, &search, &importButton, &loadButton,
             &relocateButton, &targetDeck, &list})
        addAndMakeVisible(child);

    updateButtons();
}

void LibraryPanel::setResults(std::vector<broke::library::TrackRecord> tracks) {
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
    if (rows.empty())
        setMessage(uiText("No matching tracks.", "Brak pasujących utworów."));
    else
        setMessage(uiText("Tracks: ", "Utwory: ") + juce::String(static_cast<int>(rows.size())));
    updateButtons();
}

void LibraryPanel::setBusy(bool nextBusy) {
    busy = nextBusy;
    search.setEnabled(!busy);
    importButton.setEnabled(!busy);
    if (busy) setMessage(uiText("Working…", "Przetwarzanie…"));
    updateButtons();
}

void LibraryPanel::setMessage(const juce::String& value) {
    message.setText(value, juce::dontSendNotification);
    message.setTooltip(value);
}

void LibraryPanel::paint(juce::Graphics& g) {
    g.fillAll(background);
}

void LibraryPanel::resized() {
    auto area = getLocalBounds().reduced(14);
    auto header = area.removeFromTop(34);
    heading.setBounds(header.removeFromLeft(230));
    message.setBounds(header);
    area.removeFromTop(8);

    auto searchRow = area.removeFromTop(34);
    const int importWidth = 116;
    importButton.setBounds(searchRow.removeFromRight(importWidth).reduced(4, 0));
    search.setBounds(searchRow.reduced(0, 1));
    area.removeFromTop(8);

    auto actions = area.removeFromBottom(38);
    targetDeck.setBounds(actions.removeFromLeft(110).reduced(2, 3));
    loadButton.setBounds(actions.removeFromLeft(140).reduced(4, 3));
    relocateButton.setBounds(actions.removeFromLeft(150).reduced(4, 3));
    list.setBounds(area);
}

int LibraryPanel::getNumRows() {
    return static_cast<int>(rows.size());
}

void LibraryPanel::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
                                    bool rowIsSelected) {
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

void LibraryPanel::selectedRowsChanged(int) {
    updateButtons();
}

void LibraryPanel::listBoxItemDoubleClicked(int row, const juce::MouseEvent&) {
    if (row >= 0 && row < static_cast<int>(rows.size())) {
        list.selectRow(row);
        loadSelected();
    }
}

const broke::library::TrackRecord* LibraryPanel::selectedTrack() const noexcept {
    const int selected = list.getSelectedRow();
    if (selected < 0 || selected >= static_cast<int>(rows.size())) return nullptr;
    return &rows[static_cast<std::size_t>(selected)];
}

void LibraryPanel::loadSelected() {
    if (busy) return;
    const auto* selected = selectedTrack();
    const int target = targetDeck.getSelectedId();
    if (selected != nullptr && target >= 1 && target <= 4 && onLoadTrack)
        onLoadTrack(*selected, static_cast<std::size_t>(target - 1));
}

void LibraryPanel::updateButtons() {
    const auto* selected = selectedTrack();
    loadButton.setEnabled(!busy && selected != nullptr && !selected->missing);
    relocateButton.setEnabled(!busy && selected != nullptr);
}
