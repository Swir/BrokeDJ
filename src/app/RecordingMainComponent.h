// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

// Keep the established recording/mixer implementation intact while layering
// M4 runtime library behavior around it. The copied base header is the previous
// RecordingMainComponent implementation; the public type below remains the app
// surface consumed by Main.cpp.
#define RecordingMainComponent RecordingMainComponentBase
#include "RecordingMainComponentBase.h"
#undef RecordingMainComponent

#include "LibraryDatabase.h"
#include "LibraryRuntimeStore.h"

#include <JuceHeader.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class RecordingMainComponent final : public juce::Component, private juce::Timer {
public:
    explicit RecordingMainComponent(bool openAudio = true, bool enableKeyLockResearch = false)
        : base(openAudio, enableKeyLockResearch), databaseFile(libraryDatabaseFile()) {
        addAndMakeVisible(base);

        historyButton.setButtonText(text("HISTORY", "HISTORIA"));
        historyButton.setTooltip(text(
            "Review recent local playback starts. History stays in BrokeDJ's local SQLite library.",
            "Przeglądaj ostatnie lokalne uruchomienia odtwarzania. Historia zostaje w lokalnej bazie SQLite BrokeDJ."));
        historyButton.onClick = [this] { showHistory(); };
        addAndMakeVisible(historyButton);

        std::string error;
        historyDatabaseAvailable = historyDatabase.open(databaseFile, &error);
        if (historyDatabaseAvailable) {
            scheduleHashBackfill();
        } else {
            historyButton.setEnabled(false);
            historyButton.setTooltip(text(
                "Playback history is unavailable because the local library database could not be opened.",
                "Historia odtwarzania jest niedostępna, ponieważ nie udało się otworzyć lokalnej bazy biblioteki."));
        }

        setSize(base.getWidth(), base.getHeight());
        startTimerHz(20);
    }

    ~RecordingMainComponent() override {
        stopTimer();
        cancelled.store(true, std::memory_order_release);
        if (historyDialog) delete historyDialog.getComponent();
        historyWorker.removeAllJobs(true, -1);
        maintenanceWorker.removeAllJobs(true, -1);
        historyDatabase.close();
    }

    void resized() override {
        base.setBounds(getLocalBounds());

        // Mirror the base top-bar geometry and occupy the reserved gap directly
        // left of Library. At the app's minimum width this remains clear of the
        // BrokeDJ title while preserving the existing controls.
        constexpr int settingsWidth = 165;
        constexpr int gap = 6;
        int right = getWidth() - 20 - settingsWidth - 8;
        right -= 92 + gap;   // REC SET
        right -= 58 + gap;   // LIMIT
        right -= 52 + gap;   // MIC
        right -= 62 + gap;   // BOOTH
        right -= 130 + gap;  // Booth level
        right -= 64 + gap;   // MIC I/O
        right -= 82 + gap;   // Library
        constexpr int historyWidth = 82;
        historyButton.setBounds(std::max(170, right - historyWidth), 20, historyWidth, 34);
        historyButton.toFront(false);
    }

private:
    class HistoryPanel final : public juce::Component, private juce::ListBoxModel {
    public:
        HistoryPanel() : list("BrokeDJ playback history", this) {
            heading.setText(text("PLAYBACK HISTORY", "HISTORIA ODTWARZANIA"),
                            juce::dontSendNotification);
            heading.setFont(juce::Font(juce::FontOptions(18.0f).withStyle("Bold")));
            heading.setColour(juce::Label::textColourId, pale);

            message.setText(text("Recent playback starts stored locally.",
                                 "Ostatnie uruchomienia odtwarzania zapisane lokalnie."),
                            juce::dontSendNotification);
            message.setColour(juce::Label::textColourId, muted);

            refresh.setButtonText(text("Refresh", "Odśwież"));
            refresh.onClick = [this] { if (onRefresh) onRefresh(); };

            list.setRowHeight(46);
            list.setColour(juce::ListBox::backgroundColourId, background);
            list.setColour(juce::ListBox::outlineColourId, blue.withAlpha(0.25f));
            list.setOutlineThickness(1);

            addAndMakeVisible(heading);
            addAndMakeVisible(message);
            addAndMakeVisible(refresh);
            addAndMakeVisible(list);
        }

        std::function<void()> onRefresh;

        void setBusy(bool busy) {
            refresh.setEnabled(!busy);
            message.setText(busy
                ? text("Reading local history…", "Odczytywanie lokalnej historii…")
                : (rows.empty()
                    ? text("No playback history yet.", "Brak historii odtwarzania.")
                    : text("History entries: ", "Wpisy historii: ")
                        + juce::String(static_cast<int>(rows.size()))),
                juce::dontSendNotification);
        }

        void setRows(std::vector<broke::library::runtime::PlayedTrackRow> nextRows,
                     bool ok) {
            rows = ok ? std::move(nextRows) : std::vector<broke::library::runtime::PlayedTrackRow>{};
            list.updateContent();
            refresh.setEnabled(true);
            message.setText(ok
                ? (rows.empty()
                    ? text("No playback history yet.", "Brak historii odtwarzania.")
                    : text("History entries: ", "Wpisy historii: ")
                        + juce::String(static_cast<int>(rows.size())))
                : text("Could not read local playback history.",
                       "Nie udało się odczytać lokalnej historii odtwarzania."),
                juce::dontSendNotification);
        }

        void resized() override {
            auto area = getLocalBounds().reduced(14);
            auto header = area.removeFromTop(36);
            refresh.setBounds(header.removeFromRight(104).reduced(3, 2));
            heading.setBounds(header.removeFromLeft(std::min(280, header.getWidth() / 2)));
            message.setBounds(header);
            area.removeFromTop(8);
            list.setBounds(area);
        }

        void paint(juce::Graphics& graphics) override { graphics.fillAll(background); }

    private:
        int getNumRows() override { return static_cast<int>(rows.size()); }

        static juce::String displayTitle(const broke::library::TrackRecord& track) {
            juce::String titleText = juce::String::fromUTF8(track.title.c_str());
            if (titleText.isEmpty()) {
                titleText = juce::File(juce::String::fromUTF8(track.path.c_str()))
                                .getFileNameWithoutExtension();
            }
            const auto artistText = juce::String::fromUTF8(track.artist.c_str());
            if (artistText.isNotEmpty()) titleText = artistText + " — " + titleText;
            if (track.missing) titleText += text("  [MISSING]", "  [BRAK PLIKU]");
            return titleText;
        }

        void paintListBoxItem(int rowNumber, juce::Graphics& graphics, int width, int height,
                              bool rowIsSelected) override {
            if (rowNumber < 0 || rowNumber >= static_cast<int>(rows.size())) return;
            const auto& row = rows[static_cast<std::size_t>(rowNumber)];
            if (rowIsSelected) graphics.fillAll(blue.withAlpha(0.22f));
            else if ((rowNumber & 1) != 0) graphics.fillAll(panel.withAlpha(0.55f));

            auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(9, 3);
            graphics.setColour(row.track.missing ? muted : pale);
            graphics.setFont(13.0f);
            graphics.drawFittedText(displayTitle(row.track), bounds.removeFromTop(20),
                                    juce::Justification::centredLeft, 1);

            const juce::Time when(row.playedAtUnixMs);
            const auto timestamp = when.toString(true, true, false, true);
            const auto detail = timestamp + "  ·  " + juce::String::fromUTF8(row.track.path.c_str());
            graphics.setColour(muted);
            graphics.setFont(10.5f);
            graphics.drawFittedText(detail, bounds, juce::Justification::centredLeft, 1);
        }

        const juce::Colour background{0xff080e1a};
        const juce::Colour panel{0xff111d30};
        const juce::Colour blue{0xff3d9bff};
        const juce::Colour pale{0xffdcecff};
        const juce::Colour muted{0xff8199b8};
        juce::Label heading, message;
        juce::TextButton refresh;
        juce::ListBox list;
        std::vector<broke::library::runtime::PlayedTrackRow> rows;
    };

    static std::filesystem::path libraryDatabaseFile() {
        const auto file = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                              .getChildFile("BrokeDJ")
                              .getChildFile("library.sqlite3");
#if JUCE_WINDOWS
        return std::filesystem::path(file.getFullPathName().toWideCharPointer());
#else
        return std::filesystem::u8path(file.getFullPathName().toStdString());
#endif
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

    void timerCallback() override {
        if (!historyDatabaseAvailable || cancelled.load(std::memory_order_acquire)) return;
        const auto state = base.captureSessionState();
        for (std::size_t deck = 0; deck < state.decks.size(); ++deck) {
            const auto& deckState = state.decks[deck];
            const bool playing = deckState.wasPlaying && !deckState.path.empty();
            if (playing && !previousPlaying[deck]) {
                recordPlaybackStart(juce::File(juce::String::fromUTF8(deckState.path.c_str())));
            }
            previousPlaying[deck] = playing;
        }
    }

    void recordPlaybackStart(const juce::File& file) {
        if (!file.existsAsFile()) return;
        const auto normalizedPath = utf8Path(file);
        const auto playedAt = juce::Time::currentTimeMillis();
        juce::Component::SafePointer<RecordingMainComponent> safe(this);
        historyWorker.addJob([safe, file, normalizedPath, playedAt] {
            if (!safe || safe->cancelled.load(std::memory_order_acquire)) return;
            std::string error;
            auto track = broke::library::runtime::findTrackByPath(
                safe->databaseFile, normalizedPath, &error);
            std::optional<std::int64_t> trackId;
            if (track) {
                trackId = track->id;
            } else if (error.empty()) {
                broke::library::TrackRecord record;
                record.path = normalizedPath;
                record.fileSize = std::max<std::int64_t>(0, file.getSize());
                record.modifiedNs = modifiedNs(file);
                record.title = file.getFileNameWithoutExtension().toStdString();
                trackId = safe->historyDatabase.upsertTrack(record, &error);
            }
            if (!trackId || !error.empty()) return;
            if (!safe->historyDatabase.recordPlay(*trackId, playedAt, &error) || !error.empty()) return;

            juce::MessageManager::callAsync([safe] {
                if (safe && safe->historyPanel) safe->refreshHistory();
            });
        });
    }

    void scheduleHashBackfill() {
        juce::Component::SafePointer<RecordingMainComponent> safe(this);
        maintenanceWorker.addJob([safe] {
            if (!safe || safe->cancelled.load(std::memory_order_acquire)) return;
            std::string error;
            const auto result = broke::library::runtime::backfillContentHashes(
                safe->databaseFile, 64, &safe->cancelled, &error);
            if (!safe || safe->cancelled.load(std::memory_order_acquire)) return;
            juce::Logger::writeToLog(
                "BrokeDJ library identity backfill: scanned=" + juce::String(static_cast<int>(result.scanned))
                + " hashed=" + juce::String(static_cast<int>(result.hashed))
                + " missing=" + juce::String(static_cast<int>(result.markedMissing))
                + " skipped=" + juce::String(static_cast<int>(result.skipped))
                + " failed=" + juce::String(static_cast<int>(result.failed))
                + (error.empty() ? juce::String{} : " status=partial"));
        });
    }

    void showHistory() {
        if (!historyDatabaseAvailable) return;
        if (historyDialog) {
            historyDialog->toFront(true);
            refreshHistory();
            return;
        }

        auto* content = new HistoryPanel();
        juce::Component::SafePointer<RecordingMainComponent> safe(this);
        content->onRefresh = [safe] { if (safe) safe->refreshHistory(); };

        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = text("BrokeDJ / Playback history", "BrokeDJ / Historia odtwarzania");
        options.dialogBackgroundColour = juce::Colour(0xff080e1a);
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.escapeKeyTriggersCloseButton = true;
        options.content.setOwned(content);
        options.content->setSize(820, 520);
        options.componentToCentreAround = this;
        historyPanel = content;
        historyDialog = options.launchAsync();
        refreshHistory();
    }

    void refreshHistory() {
        if (!historyPanel || cancelled.load(std::memory_order_acquire)) return;
        historyPanel->setBusy(true);
        const auto generation = historyGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
        juce::Component::SafePointer<RecordingMainComponent> safe(this);
        historyWorker.addJob([safe, generation] {
            if (!safe || safe->cancelled.load(std::memory_order_acquire)) return;
            std::string error;
            auto rows = broke::library::runtime::recentPlayedTracks(
                safe->databaseFile, 250, &error);
            const bool ok = error.empty();
            juce::MessageManager::callAsync(
                [safe, generation, rows = std::move(rows), ok]() mutable {
                    if (!safe || safe->cancelled.load(std::memory_order_acquire)
                        || safe->historyGeneration.load(std::memory_order_acquire) != generation
                        || !safe->historyPanel) return;
                    safe->historyPanel->setRows(std::move(rows), ok);
                });
        });
    }

    RecordingMainComponentBase base;
    juce::TextButton historyButton;
    const std::filesystem::path databaseFile;
    broke::library::LibraryDatabase historyDatabase;
    juce::ThreadPool historyWorker{1};
    juce::ThreadPool maintenanceWorker{1};
    std::atomic<bool> cancelled{false};
    std::atomic<std::uint64_t> historyGeneration{0};
    std::array<bool, broke::session::SessionState::deckCount> previousPlaying{};
    juce::Component::SafePointer<juce::DialogWindow> historyDialog;
    juce::Component::SafePointer<HistoryPanel> historyPanel;
    bool historyDatabaseAvailable = false;
};
