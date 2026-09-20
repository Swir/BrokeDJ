// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "MainComponent.h"
#include "SetRecorder.h"

#include <atomic>
#include <cmath>
#include <memory>

// Thin native wrapper that records the already-rendered master outputs 1/2.
// MainComponent keeps ownership of playback/device logic; SetRecorder owns only
// the bounded realtime copy and background WAV finalization path.
class RecordingMainComponent final : public MainComponent {
public:
    explicit RecordingMainComponent(bool openAudio = true, bool enableKeyLockResearch = false)
        : MainComponent(openAudio, enableKeyLockResearch) {
        recordButton.setButtonText(text("REC SET", "NAGRAJ SET"));
        recordButton.setTooltip(text(
            "Record master outputs 1/2 to a 24-bit WAV. Disk encoding runs on a background thread; FIFO overflow is counted as a recording dropout instead of blocking playback.",
            "Nagraj wyjście master 1/2 do WAV 24-bit. Zapis na dysk działa w tle; przepełnienie bufora jest liczone jako dropout nagrania zamiast blokować odtwarzanie."));
        recordButton.onClick = [this] { toggleRecording(); };
        addAndMakeVisible(recordButton);
    }

    ~RecordingMainComponent() override {
        recordChooser.reset();
        recorder.stop();
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override {
        MainComponent::prepareToPlay(samplesPerBlockExpected, sampleRate);
        preparedSampleRate.store(std::isfinite(sampleRate) ? sampleRate : 0.0,
                                 std::memory_order_release);
    }

    void releaseResources() override {
        recorder.stop();
        preparedSampleRate.store(0.0, std::memory_order_release);
        MainComponent::releaseResources();
        juce::MessageManager::callAsync([safe = juce::Component::SafePointer<RecordingMainComponent>(this)] {
            if (!safe) return;
            safe->recordButton.setToggleState(false, juce::dontSendNotification);
            safe->recordButton.setButtonText(text("REC SET", "NAGRAJ SET"));
        });
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override {
        MainComponent::getNextAudioBlock(info);
        if (!recorder.isRecording() || info.buffer == nullptr || info.numSamples <= 0) return;
        if (info.buffer->getNumChannels() < 2) {
            recorder.capture(nullptr, nullptr, info.numSamples);
            return;
        }
        recorder.capture(info.buffer->getReadPointer(0, info.startSample),
                         info.buffer->getReadPointer(1, info.startSample),
                         info.numSamples);
    }

    void resized() override {
        MainComponent::resized();
        constexpr int buttonWidth = 108;
        constexpr int settingsWidth = 165;
        recordButton.setBounds(getWidth() - 20 - settingsWidth - 8 - buttonWidth,
                               20, buttonWidth, 34);
    }

private:
    void toggleRecording() {
        if (recorder.isRecording()) {
            stopRecordingAndReport();
            return;
        }

        const double rate = preparedSampleRate.load(std::memory_order_acquire);
        if (!std::isfinite(rate) || rate < 8000.0) {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                text("Recording unavailable", "Nagrywanie niedostępne"),
                text("Select a working audio device before recording a set.",
                     "Wybierz działające urządzenie audio przed nagrywaniem setu."));
            return;
        }

        if (recordChooser) return;
        const auto stamp = juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S");
        auto directory = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
        if (!directory.isDirectory())
            directory = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        const auto suggested = directory.getChildFile("BrokeDJ-set-" + stamp + ".wav");
        recordChooser = std::make_unique<juce::FileChooser>(
            text("Record BrokeDJ set", "Nagraj set BrokeDJ"), suggested, "*.wav");

        juce::Component::SafePointer<RecordingMainComponent> safe(this);
        recordChooser->launchAsync(
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                | juce::FileBrowserComponent::warnAboutOverwriting,
            [safe, rate](const juce::FileChooser& chooser) {
                const auto selected = chooser.getResult();
                if (!safe) return;
                safe->recordChooser.reset();
                if (selected == juce::File{}) return;
                safe->startRecording(selected, rate);
            });
    }

    void startRecording(juce::File selected, double rate) {
        if (selected.getFileExtension().toLowerCase() != ".wav")
            selected = selected.withFileExtension("wav");
        if (selected.exists()) selected = selected.getNonexistentSibling(false);

        if (!recorder.start(selected, rate)) {
            const auto state = recorder.snapshot();
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                text("Recording could not start", "Nie można rozpocząć nagrywania"),
                state.error.isNotEmpty() ? state.error
                                         : text("The WAV recording path could not be prepared.",
                                                "Nie udało się przygotować ścieżki nagrania WAV."));
            return;
        }
        const auto state = recorder.snapshot();
        recordButton.setToggleState(true, juce::dontSendNotification);
        recordButton.setButtonText(text("STOP REC", "STOP NAGR."));
        recordButton.setTooltip(text("Recording master 1/2 to: ", "Nagrywanie master 1/2 do: ")
                                + state.destination.getFullPathName());
    }

    void stopRecordingAndReport() {
        recorder.stop();
        const auto state = recorder.snapshot();
        recordButton.setToggleState(false, juce::dontSendNotification);
        recordButton.setButtonText(text("REC SET", "NAGRAJ SET"));

        juce::String detail;
        if (state.finalized) {
            detail = text("Saved: ", "Zapisano: ") + state.destination.getFullPathName();
        } else if (state.recoveryFile.existsAsFile()) {
            detail = text("Finalization failed; recovery file kept: ",
                          "Finalizacja nie powiodła się; zachowano plik odzyskiwania: ")
                + state.recoveryFile.getFullPathName();
        } else {
            detail = state.error.isNotEmpty() ? state.error
                                              : text("Recording stopped without a finalized file.",
                                                     "Nagrywanie zatrzymano bez finalnego pliku.");
        }
        detail << text(" | written frames: ", " | zapisane klatki: ")
               << static_cast<juce::int64>(state.writtenFrames)
               << text(" | dropped frames: ", " | pominięte klatki: ")
               << static_cast<juce::int64>(state.droppedFrames)
               << text(" | dropout events: ", " | zdarzenia dropout: ")
               << static_cast<juce::int64>(state.dropoutEvents);
        recordButton.setTooltip(detail);

        const auto icon = state.finalized && state.dropoutEvents == 0
            ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon;
        juce::AlertWindow::showMessageBoxAsync(
            icon,
            state.finalized
                ? text("Set recording finished", "Nagrywanie setu zakończone")
                : text("Set recording needs attention", "Nagranie setu wymaga uwagi"),
            detail);
    }

    SetRecorder recorder;
    juce::TextButton recordButton;
    std::unique_ptr<juce::FileChooser> recordChooser;
    std::atomic<double> preparedSampleRate{0.0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordingMainComponent)
};
