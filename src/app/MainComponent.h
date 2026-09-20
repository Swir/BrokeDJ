// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <JuceHeader.h>
#include "core/Engine.h"
#include "core/PerformanceDeckOwner.h"
#include "core/TempoSegmentEditor.h"
#include "Decoder.h"
#include "PerformanceStateStore.h"
#include "TrackAnalysis.h"
#include "TempoSegmentEditorComponent.h"
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
#include "KeyLockDeckLifecycle.h"
#endif

#include <cstdint>
#include <optional>

juce::String text(const char* english, const char* polish);
class Waveform final : public juce::Component {
public:
    std::vector<float> peaks;
    float progress = 0;
    std::function<void(double)> onSeek;
    void setBeatGrid(const broke::BeatGrid&, bool manual);
    void setDuration(double seconds) noexcept;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent& event) override;
private:
    broke::BeatGrid beatGrid;
    double durationSeconds = 0.0;
    bool manualGrid = false;
};
class DeckPanel final : public juce::Component, public juce::FileDragAndDropTarget {
public:
    DeckPanel(broke::Engine&, std::size_t);
    std::function<void()> onBrowse;
    std::function<void(const juce::File&)> onDrop;
    std::function<void()> onBeforePlay;
    std::function<void()> onKeyLockControlChanged;
    std::function<void(double)> onSeekRequested;
    std::function<void(double, double)> onGridEdit;
    std::function<void()> onGridReset;
    std::function<void()> onTempoMapRequested;
    std::function<void(bool)> onWholeTrackLoopRequested;
    std::function<bool(double, bool)> onBeatLoopRequested;
    std::function<void(std::size_t, bool)> onHotCueRequested;
    std::function<void(double)> onBeatJumpRequested;
    std::function<void(bool)> onSyncMasterRequested;
    std::function<void()> onSyncRequested;
    std::function<bool(broke::PerformanceDeckOwner::ReverseSlipMode)> onReverseSlipModeRequested;
    void setTrack(const juce::String&, std::vector<float>);
    void setLoading(bool);
    void setRhythmPending();
    void setRhythmAnalysis(const TrackRhythmAnalysis&, const broke::BeatGrid&, bool manual);
    void setBeatGrid(const broke::BeatGrid&, bool manual);
    void setPerformanceState(bool gridAvailable, bool beatLoopIsActive, double beatLoopBeats,
                             const broke::PerformanceDeckOwner::HotCueBank& hotCues,
                             bool trackReady, bool isSyncMaster, bool syncAvailable);
    void refresh();
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;
private:
    void refreshRhythmDisplay();
    broke::Engine& engine;
    std::size_t index;
    juce::Label heading, track, time, rhythm;
    Waveform waveform;
    juce::TextButton load, play, rewind, loop, beatLoop, cue;
    juce::ComboBox beatLoopLength;
    std::array<juce::TextButton, broke::PerformanceDeckOwner::hotCueCount> hotCuePads;
    juce::TextButton jumpBack, jumpForward, reverse, slip, syncMaster, sync;
    juce::ComboBox jumpLength;
    std::array<juce::Slider, 7> knobs;
    std::array<juce::Label, 7> knobNames;
    juce::Slider gridZero, gridBpm;
    juce::Label gridZeroLabel, gridBpmLabel;
    juce::TextButton gridReset, tempoMap;
    TrackRhythmAnalysis rhythmAnalysis;
    broke::BeatGrid activeBeatGrid;
    bool rhythmReady = false;
    bool manualBeatGrid = false;
};
class MainComponent final : public juce::AudioAppComponent, private juce::Timer {
public:
    explicit MainComponent(bool openAudio = true, bool enableKeyLockResearch = false);
    ~MainComponent() override;
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo&) override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void browse(std::size_t);
    void load(std::size_t, const juce::File&);
    void startTrackAnalysis(std::size_t, const juce::File&);
    void applyBeatGridEdit(std::size_t, double beatZeroSeconds, double bpm);
    void adoptAndPersistReviewedGrid(std::size_t, const broke::BeatGrid&, const juce::String& successMessage);
    void acceptTempoSegmentEdit(std::size_t);
    void resetBeatGridEdit(std::size_t);
    void showTempoSegmentEditor(std::size_t);
    void closeTempoSegmentEditorForDeck(std::size_t);
    void setWholeTrackLoop(std::size_t, bool enabled);
    [[nodiscard]] bool setBeatLoop(std::size_t, double beats, bool enabled);
    [[nodiscard]] bool setReverseSlipMode(std::size_t deck, broke::PerformanceDeckOwner::ReverseSlipMode mode);
    void handleHotCue(std::size_t deck, std::size_t slot, bool clear);
    [[nodiscard]] bool jumpBeats(std::size_t deck, double beats);
    void setSyncMaster(std::size_t deck, bool enabled);
    [[nodiscard]] bool syncDeck(std::size_t followerDeck);
    void restoreHotCues(std::size_t deck, const juce::File& file, double trackDurationSeconds,
                        std::uint64_t generation);
    void persistHotCues(std::size_t deck, const juce::File& file, std::uint64_t generation,
                        broke::PerformanceDeckOwner::HotCueBank snapshot);
    void showAudioSettings();
    void statusMessage(const juce::String&);
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
    void serviceKeyLockDeck(std::size_t deck, bool playing);
#endif
    juce::LookAndFeel_V4 theme;
    broke::Engine engine;
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
    broke::KeyLockDeckLifecycle keyLockLifecycle{engine};
    bool keyLockResearchEnabled = false;
#endif
    std::array<std::unique_ptr<broke::PerformanceDeckOwner>, broke::deckCount> performanceDecks;
    std::array<std::unique_ptr<broke::TempoSegmentEditorModel>, broke::deckCount> tempoSegmentEditors;
    std::array<std::unique_ptr<DeckPanel>, broke::deckCount> decks;
    std::optional<std::size_t> syncMasterDeck;
    std::array<bool, broke::deckCount> loading{};
    std::array<juce::File, broke::deckCount> deckFiles;
    std::array<broke::BeatGrid, broke::deckCount> detectedBeatGrids;
    std::array<broke::BeatGrid, broke::deckCount> beatGrids;
    std::array<bool, broke::deckCount> gridIsManual{};
    std::array<std::atomic<std::uint64_t>, broke::deckCount> gridEditGeneration{};
    std::array<std::atomic<std::uint64_t>, broke::deckCount> hotCueGeneration{};
    std::array<std::shared_ptr<std::atomic<bool>>, broke::deckCount> analysisCancelled{};
    std::shared_ptr<std::atomic<bool>> cancelled = std::make_shared<std::atomic<bool>>(false);
    juce::ThreadPool loaders{1};
    // Analysis/persistence is deliberately separate from decoding/playback preparation so a
    // long BPM/grid pass or hotcue state write cannot prevent another deck from becoming playable.
    // One worker serializes analysis and local state I/O, bounding CPU and keeping all cache/
    // persistence work off the audio callback.
    juce::ThreadPool analyzers{1};
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Component::SafePointer<juce::DialogWindow> audioSettings;
    juce::Component::SafePointer<juce::DialogWindow> tempoSegmentDialog;
    std::optional<std::size_t> tempoSegmentDialogDeck;
    juce::Label title, subtitle, status, crossLabel, masterLabel, cueLabel, meterLabel;
    juce::TextButton settings;
    juce::HyperlinkButton author;
    juce::Slider crossfader, master, headphone;
    std::atomic<bool> audioReady{false};
    juce::TooltipWindow tooltips{this, 600};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
