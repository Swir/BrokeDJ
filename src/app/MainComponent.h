// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <JuceHeader.h>
#include "core/Engine.h"
#include "core/PerformanceDeckOwner.h"
#include "Decoder.h"
#include "TrackAnalysis.h"
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
#include "KeyLockDeckLifecycle.h"
#endif

#include <cstdint>

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
    std::function<void(bool)> onWholeTrackLoopRequested;
    std::function<bool(double, bool)> onBeatLoopRequested;
    void setTrack(const juce::String&, std::vector<float>);
    void setLoading(bool);
    void setRhythmPending();
    void setRhythmAnalysis(const TrackRhythmAnalysis&, const broke::BeatGrid&, bool manual);
    void setBeatGrid(const broke::BeatGrid&, bool manual);
    void setPerformanceState(bool gridAvailable, bool beatLoopIsActive, double beatLoopBeats);
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
    std::array<juce::Slider, 7> knobs;
    std::array<juce::Label, 7> knobNames;
    juce::Slider gridZero, gridBpm;
    juce::Label gridZeroLabel, gridBpmLabel;
    juce::TextButton gridReset;
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
    void resetBeatGridEdit(std::size_t);
    void setWholeTrackLoop(std::size_t, bool enabled);
    [[nodiscard]] bool setBeatLoop(std::size_t, double beats, bool enabled);
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
    std::array<std::unique_ptr<DeckPanel>, broke::deckCount> decks;
    std::array<bool, broke::deckCount> loading{};
    std::array<juce::File, broke::deckCount> deckFiles;
    std::array<broke::BeatGrid, broke::deckCount> detectedBeatGrids;
    std::array<broke::BeatGrid, broke::deckCount> beatGrids;
    std::array<bool, broke::deckCount> gridIsManual{};
    std::array<std::atomic<std::uint64_t>, broke::deckCount> gridEditGeneration{};
    std::array<std::shared_ptr<std::atomic<bool>>, broke::deckCount> analysisCancelled{};
    std::shared_ptr<std::atomic<bool>> cancelled = std::make_shared<std::atomic<bool>>(false);
    juce::ThreadPool loaders{1};
    // Analysis is deliberately separate from decoding/playback preparation so a
    // long BPM/grid pass cannot prevent another deck from becoming playable.
    // One worker serializes analysis and beat-grid persistence, bounding CPU and
    // keeping all cache/override disk I/O off the audio callback.
    juce::ThreadPool analyzers{1};
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Component::SafePointer<juce::DialogWindow> audioSettings;
    juce::Label title, subtitle, status, crossLabel, masterLabel, cueLabel, meterLabel;
    juce::TextButton settings;
    juce::HyperlinkButton author;
    juce::Slider crossfader, master, headphone;
    std::atomic<bool> audioReady{false};
    juce::TooltipWindow tooltips{this, 600};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
