// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "LibraryDatabase.h"

#include <JuceHeader.h>

#include <cstddef>
#include <functional>
#include <vector>

class LibraryPanel final : public juce::Component, private juce::ListBoxModel {
public:
    LibraryPanel();

    std::function<void(const juce::String&)> onSearchChanged;
    std::function<void()> onImportFiles;
    std::function<void(const broke::library::TrackRecord&, std::size_t)> onLoadTrack;
    std::function<void(const broke::library::TrackRecord&)> onRelocateTrack;

    void setResults(std::vector<broke::library::TrackRecord> tracks);
    void setBusy(bool busy);
    void setMessage(const juce::String& message);
    [[nodiscard]] juce::String query() const { return search.getText(); }

    void resized() override;
    void paint(juce::Graphics&) override;

private:
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics&, int width, int height,
                          bool rowIsSelected) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;

    [[nodiscard]] const broke::library::TrackRecord* selectedTrack() const noexcept;
    void loadSelected();
    void updateButtons();

    juce::Label heading, message;
    juce::TextEditor search;
    juce::TextButton importButton, loadButton, relocateButton;
    juce::ComboBox targetDeck;
    juce::ListBox list;
    std::vector<broke::library::TrackRecord> rows;
    bool busy = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LibraryPanel)
};
