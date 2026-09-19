// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <JuceHeader.h>
#include "core/TempoSegmentEditor.h"

#include <array>
#include <cmath>
#include <functional>
#include <limits>

juce::String text(const char* english, const char* polish);

// Message-thread-only compact editor for reviewed variable-tempo maps.
// All mutations flow through TempoSegmentEditorModel -> PerformanceDeckOwner.
// This component performs no file I/O; MainComponent persists an accepted
// reviewed-grid snapshot asynchronously on the analysis/persistence worker.
class TempoSegmentEditorComponent final : public juce::Component {
public:
    using PositionProvider = std::function<double()>;
    using AcceptedEdit = std::function<void()>;
    using StatusSink = std::function<void(const juce::String&)>;

    TempoSegmentEditorComponent(broke::TempoSegmentEditorModel& target,
                                PositionProvider positionProvider,
                                AcceptedEdit acceptedEdit,
                                StatusSink statusSink)
        : model(target), currentPosition(std::move(positionProvider)),
          onAcceptedEdit(std::move(acceptedEdit)), onStatus(std::move(statusSink)) {
        title.setText(text("TEMPO MAP", "MAPA TEMPA"), juce::dontSendNotification);
        title.setFont(juce::Font(juce::FontOptions(18.0f).withStyle("Bold")));
        title.setJustificationType(juce::Justification::centredLeft);
        title.setColour(juce::Label::textColourId, juce::Colour(0xffdcecff));

        segmentLabel.setText(text("Segment", "Segment"), juce::dontSendNotification);
        beatLabel.setText(text("Boundary beat", "Beat granicy"), juce::dontSendNotification);
        bpmLabel.setText("BPM", juce::dontSendNotification);
        hint.setText(text(
            "Base segment: BPM only. Later boundaries can be moved, replaced or removed. Add/Move @ playhead uses the current source-time position.",
            "Segment bazowy: tylko BPM. Późniejsze granice można przesuwać, zastępować lub usuwać. Dodaj/Przesuń @ pozycja używa bieżącego czasu utworu."),
            juce::dontSendNotification);
        hint.setColour(juce::Label::textColourId, juce::Colour(0xff8199b8));
        hint.setFont(juce::Font(juce::FontOptions(11.0f)));
        hint.setJustificationType(juce::Justification::centredLeft);

        for (auto* label : {&segmentLabel, &beatLabel, &bpmLabel}) {
            label->setColour(juce::Label::textColourId, juce::Colour(0xffdcecff));
            label->setFont(juce::Font(juce::FontOptions(11.0f)));
        }

        beatEditor.setInputRestrictions(18, "-0123456789.,");
        bpmEditor.setInputRestrictions(12, "0123456789.,");
        beatEditor.setTooltip(text("Exact musical beat for the selected tempo boundary.",
                                   "Dokładny beat muzyczny wybranej granicy tempa."));
        bpmEditor.setTooltip(text("Tempo for the selected/new segment. Valid grid limits are enforced by the core model.",
                                  "Tempo wybranego/nowego segmentu. Rdzeń wymusza poprawne limity siatki."));

        addAtPlayhead.setButtonText(text("ADD @ PLAYHEAD", "DODAJ @ POZYCJA"));
        apply.setButtonText(text("APPLY", "ZASTOSUJ"));
        moveToPlayhead.setButtonText(text("MOVE @ PLAYHEAD", "PRZESUŃ @ POZYCJA"));
        remove.setButtonText(text("REMOVE", "USUŃ"));
        refresh.setButtonText(text("REFRESH", "ODŚWIEŻ"));

        segmentChoice.onChange = [this] {
            const int id = segmentChoice.getSelectedId();
            if (id <= 0) return;
            const auto result = model.select(static_cast<std::size_t>(id - 1));
            if (result != broke::PerformanceDeckOwner::Result::applied)
                reportRejected(text("Segment selection is stale; refresh and try again.",
                                    "Wybór segmentu jest nieaktualny; odśwież i spróbuj ponownie."));
            syncEditorsToSelection();
        };

        addAtPlayhead.onClick = [this] {
            bool bpmOk = false;
            const double bpm = parseNumber(bpmEditor.getText(), bpmOk);
            const double seconds = currentPosition ? currentPosition() : std::numeric_limits<double>::quiet_NaN();
            if (!bpmOk || !std::isfinite(seconds) || seconds < 0.0) {
                reportRejected(text("A valid BPM and playable current position are required.",
                                    "Wymagane są poprawne BPM i bieżąca pozycja odtwarzanego utworu."));
                return;
            }
            finishMutation(model.addAtTime(seconds, bpm),
                           text("Tempo boundary added and queued for local persistence.",
                                "Dodano granicę tempa i zlecono lokalny zapis."));
        };

        apply.onClick = [this] {
            const auto selected = model.selectedSegment();
            if (!selected) {
                reportRejected(text("Select a current tempo segment first.",
                                    "Najpierw wybierz aktualny segment tempa."));
                return;
            }
            bool bpmOk = false;
            const double bpm = parseNumber(bpmEditor.getText(), bpmOk);
            if (!bpmOk) {
                reportRejected(text("Enter a valid BPM value.", "Wpisz poprawną wartość BPM."));
                return;
            }
            if (selected->base) {
                finishMutation(model.setSelectedBpm(bpm),
                               text("Base tempo updated and queued for local persistence.",
                                    "Zmieniono tempo bazowe i zlecono lokalny zapis."));
                return;
            }
            bool beatOk = false;
            const double beat = parseNumber(beatEditor.getText(), beatOk);
            if (!beatOk) {
                reportRejected(text("Enter a valid boundary beat.", "Wpisz poprawny beat granicy."));
                return;
            }
            finishMutation(model.replaceSelected(beat, bpm),
                           text("Tempo segment updated and queued for local persistence.",
                                "Zmieniono segment tempa i zlecono lokalny zapis."));
        };

        moveToPlayhead.onClick = [this] {
            const double seconds = currentPosition ? currentPosition() : std::numeric_limits<double>::quiet_NaN();
            if (!std::isfinite(seconds) || seconds < 0.0) {
                reportRejected(text("The current transport position is not available.",
                                    "Bieżąca pozycja transportu jest niedostępna."));
                return;
            }
            finishMutation(model.moveSelectedToTime(seconds),
                           text("Tempo boundary moved to the playhead and queued for local persistence.",
                                "Przesunięto granicę tempa do bieżącej pozycji i zlecono lokalny zapis."));
        };

        remove.onClick = [this] {
            finishMutation(model.removeSelected(),
                           text("Tempo boundary removed and queued for local persistence.",
                                "Usunięto granicę tempa i zlecono lokalny zapis."));
        };

        refresh.onClick = [this] { refreshFromModel(); };

        const std::array<juce::Component*, 13> children{
            &title, &segmentLabel, &segmentChoice, &beatLabel, &beatEditor, &bpmLabel, &bpmEditor,
            &addAtPlayhead, &apply, &moveToPlayhead, &remove, &refresh, &hint};
        for (auto* child : children) addAndMakeVisible(child);

        setSize(680, 230);
        refreshFromModel();
    }

    void refreshFromModel() {
        const auto rows = model.segments();
        const auto selectedBefore = model.selectedIndex();

        segmentChoice.clear(juce::dontSendNotification);
        for (const auto& row : rows) {
            juce::String label = "#" + juce::String(static_cast<int>(row.index + 1));
            if (row.base) label += text(" BASE", " BAZA");
            label += "  ·  beat " + juce::String(row.beat, 3)
                + "  ·  " + juce::String(row.bpm, 2) + " BPM";
            segmentChoice.addItem(label, static_cast<int>(row.index + 1));
        }

        if (rows.empty()) {
            model.clearSelection();
            segmentChoice.setText(text("No reviewed tempo map", "Brak zweryfikowanej mapy tempa"), juce::dontSendNotification);
        } else if (selectedBefore && *selectedBefore < rows.size()) {
            segmentChoice.setSelectedId(static_cast<int>(*selectedBefore + 1), juce::dontSendNotification);
        } else {
            static_cast<void>(model.select(0));
            segmentChoice.setSelectedId(1, juce::dontSendNotification);
        }

        syncEditorsToSelection();
    }

    void resized() override {
        auto area = getLocalBounds().reduced(14);
        title.setBounds(area.removeFromTop(26));
        area.removeFromTop(4);

        auto selectRow = area.removeFromTop(42);
        segmentLabel.setBounds(selectRow.removeFromLeft(72));
        segmentChoice.setBounds(selectRow.reduced(2, 5));
        area.removeFromTop(4);

        auto editRow = area.removeFromTop(42);
        beatLabel.setBounds(editRow.removeFromLeft(96));
        beatEditor.setBounds(editRow.removeFromLeft(160).reduced(2, 5));
        bpmLabel.setBounds(editRow.removeFromLeft(44));
        bpmEditor.setBounds(editRow.removeFromLeft(130).reduced(2, 5));
        refresh.setBounds(editRow.removeFromRight(96).reduced(2, 5));
        area.removeFromTop(6);

        auto actions = area.removeFromTop(32);
        std::array<juce::Component*, 4> buttons{&addAtPlayhead, &apply, &moveToPlayhead, &remove};
        const int width = actions.getWidth() / static_cast<int>(buttons.size());
        for (auto* button : buttons)
            button->setBounds(actions.removeFromLeft(width).reduced(2, 0));
        area.removeFromTop(6);
        hint.setBounds(area);
    }

private:
    static double parseNumber(juce::String value, bool& ok) {
        value = value.trim().replaceCharacter(',', '.');
        if (value.isEmpty()) {
            ok = false;
            return 0.0;
        }
        const double parsed = value.getDoubleValue();
        ok = std::isfinite(parsed);
        return parsed;
    }

    void syncEditorsToSelection() {
        const auto selected = model.selectedSegment();
        const bool available = selected.has_value();
        segmentChoice.setEnabled(model.available());
        addAtPlayhead.setEnabled(model.available());
        apply.setEnabled(available);
        beatEditor.setEnabled(available && !selected->base);
        moveToPlayhead.setEnabled(available && !selected->base);
        remove.setEnabled(available && !selected->base);
        refresh.setEnabled(true);

        if (!available) {
            beatEditor.clear();
            bpmEditor.clear();
            bpmEditor.setEnabled(false);
            return;
        }

        bpmEditor.setEnabled(true);
        beatEditor.setText(juce::String(selected->beat, 6), false);
        bpmEditor.setText(juce::String(selected->bpm, 4), false);
    }

    void finishMutation(broke::PerformanceDeckOwner::Result result, const juce::String& success) {
        if (result == broke::PerformanceDeckOwner::Result::applied) {
            if (onAcceptedEdit) onAcceptedEdit();
            if (onStatus) onStatus(success);
            refreshFromModel();
            return;
        }
        if (result == broke::PerformanceDeckOwner::Result::gridUnavailable)
            reportRejected(text("The reviewed tempo map is no longer available.",
                                "Zweryfikowana mapa tempa nie jest już dostępna."));
        else
            reportRejected(text("Tempo-map edit was rejected safely; refresh the segment list and check the values.",
                                "Edycja mapy tempa została bezpiecznie odrzucona; odśwież listę i sprawdź wartości."));
    }

    void reportRejected(const juce::String& message) {
        if (onStatus) onStatus(message);
        refreshFromModel();
    }

    broke::TempoSegmentEditorModel& model;
    PositionProvider currentPosition;
    AcceptedEdit onAcceptedEdit;
    StatusSink onStatus;
    juce::Label title, segmentLabel, beatLabel, bpmLabel, hint;
    juce::ComboBox segmentChoice;
    juce::TextEditor beatEditor, bpmEditor;
    juce::TextButton addAtPlayhead, apply, moveToPlayhead, remove, refresh;
};
