// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <JuceHeader.h>
#include "core/Engine.h"
#include "core/PerformanceDeckOwner.h"
#include "core/TempoSegmentEditor.h"
#include "Decoder.h"
#include "PerformanceStateStore.h"
#include "SessionStore.h"
#include "TrackAnalysis.h"
#include "TempoSegmentEditorComponent.h"
#include "NativeJogScratchControl.h"
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
#include "KeyLockDeckLifecycle.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

juce::String text(const char* english, const char* polish);

class CrossfaderSlider final : public juce::Slider {
public:
    explicit CrossfaderSlider(broke::Engine& targetEngine, bool enableCurveMenu = true)
        : engine(targetEngine), curveMenuEnabled(enableCurveMenu) {
        if (curveMenuEnabled) refreshTooltip();
    }

    void mouseDown(const juce::MouseEvent& event) override {
        if (!curveMenuEnabled || !event.mods.isPopupMenu()) {
            juce::Slider::mouseDown(event);
            return;
        }

        const auto current = broke::crossfaderCurveFromRaw(
            engine.crossfaderCurve.load(std::memory_order_acquire));
        juce::PopupMenu menu;
        menu.addItem(1, text("Constant power", "Stała moc"), true,
                     current == broke::CrossfaderCurve::constantPower);
        menu.addItem(2, text("Linear", "Liniowa"), true,
                     current == broke::CrossfaderCurve::linear);
        menu.addItem(3, text("Fast cut", "Szybkie cięcie"), true,
                     current == broke::CrossfaderCurve::fastCut);

        juce::Component::SafePointer<CrossfaderSlider> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                           [safe](int result) {
            if (!safe || result < 1 || result > 3) return;
            const auto curve = result == 1 ? broke::CrossfaderCurve::constantPower
                : result == 2 ? broke::CrossfaderCurve::linear
                              : broke::CrossfaderCurve::fastCut;
            safe->setCurve(curve);
        });
    }

private:
    static juce::String curveName(broke::CrossfaderCurve curve) {
        switch (curve) {
            case broke::CrossfaderCurve::linear:
                return text("Linear", "Liniowa");
            case broke::CrossfaderCurve::fastCut:
                return text("Fast cut", "Szybkie cięcie");
            case broke::CrossfaderCurve::constantPower:
            default:
                return text("Constant power", "Stała moc");
        }
    }

    void setCurve(broke::CrossfaderCurve curve) {
        engine.crossfaderCurve.store(static_cast<std::uint8_t>(curve), std::memory_order_release);
        refreshTooltip();
    }

    void refreshTooltip() {
        const auto current = broke::crossfaderCurveFromRaw(
            engine.crossfaderCurve.load(std::memory_order_acquire));
        setTooltip(text("Crossfader curve: ", "Krzywa crossfadera: ") + curveName(current)
                   + text(". Right-click to change.", ". Kliknij prawym przyciskiem, aby zmienić."));
    }

    broke::Engine& engine;
    bool curveMenuEnabled = true;
};

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
    std::function<bool(bool)> onSyncRequested;
    std::function<bool(broke::PerformanceDeckOwner::ReverseSlipMode)> onReverseSlipModeRequested;
    void setTrack(const juce::String&, std::vector<float>);
    void setLoading(bool);
    void setRhythmPending();
    void setRhythmAnalysis(const TrackRhythmAnalysis&, const broke::BeatGrid&, bool manual);
    void setBeatGrid(const broke::BeatGrid&, bool manual);
    void setPerformanceState(bool gridAvailable, bool beatLoopIsActive, double beatLoopBeats,
                             const broke::PerformanceDeckOwner::HotCueBank& hotCues,
                             bool trackReady, bool isSyncMaster, bool syncAvailable, bool syncLocked);
    [[nodiscard]] bool jogScratchActive() const noexcept { return jogScratch.active(); }
    void syncSessionControls(const broke::session::DeckState& state) {
        loop.setToggleState(state.wholeTrackLoop, juce::dontSendNotification);
        cue.setToggleState(state.headphoneCue, juce::dontSendNotification);
        beatLoop.setToggleState(false, juce::dontSendNotification);
        reverse.setToggleState(false, juce::dontSendNotification);
        slip.setToggleState(false, juce::dontSendNotification);
        syncMaster.setToggleState(false, juce::dontSendNotification);
        sync.setToggleState(false, juce::dontSendNotification);
        knobs[0].setValue(juce::jlimit<double>(broke::minTrimDb, broke::maxTrimDb, state.trimDb),
                          juce::dontSendNotification);
        knobs[1].setValue(juce::jlimit(0.0, 1.5, static_cast<double>(state.channelGain)),
                          juce::dontSendNotification);
        knobs[2].setValue(juce::jlimit(-20.0, 20.0,
                          (static_cast<double>(state.playbackRate) - 1.0) * 100.0),
                          juce::dontSendNotification);
        knobs[3].setValue(juce::jlimit(0.0, 2.0, static_cast<double>(state.low)),
                          juce::dontSendNotification);
        knobs[4].setValue(juce::jlimit(0.0, 2.0, static_cast<double>(state.mid)),
                          juce::dontSendNotification);
        knobs[5].setValue(juce::jlimit(0.0, 2.0, static_cast<double>(state.high)),
                          juce::dontSendNotification);
        knobs[6].setValue(juce::jlimit(0.0, 0.7, static_cast<double>(state.echo)),
                          juce::dontSendNotification);
        knobs[7].setValue(juce::jlimit(0.0, 6.0, static_cast<double>(state.drive)),
                          juce::dontSendNotification);
    }
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
    NativeJogScratchControl jogScratch{*this, waveform, engine, index};
    juce::TextButton load, play, rewind, loop, beatLoop, cue;
    juce::ComboBox beatLoopLength;
    std::array<juce::TextButton, broke::PerformanceDeckOwner::hotCueCount> hotCuePads;
    juce::TextButton jumpBack, jumpForward, reverse, slip, syncMaster, sync;
    juce::ComboBox jumpLength;
    std::array<juce::Slider, 8> knobs;
    std::array<juce::Label, 8> knobNames;
    juce::Slider gridZero, gridBpm;
    juce::Label gridZeroLabel, gridBpmLabel;
    juce::TextButton gridReset, tempoMap;
    TrackRhythmAnalysis rhythmAnalysis;
    broke::BeatGrid activeBeatGrid;
    bool rhythmReady = false;
    bool manualBeatGrid = false;
};
class MainComponent : public juce::AudioAppComponent, private juce::Timer {
public:
    explicit MainComponent(bool openAudio = true, bool enableKeyLockResearch = false);
    ~MainComponent() override;
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo&) override;
    void paint(juce::Graphics&) override;
    void resized() override;

    [[nodiscard]] bool loadFileIntoDeck(std::size_t deck, const juce::File& file) {
        if (deck >= broke::deckCount || loading[deck] || !file.existsAsFile()) return false;
        load(deck, file);
        return true;
    }

    [[nodiscard]] broke::session::SessionState captureSessionState() {
        broke::session::SessionState state;
        state.mixer.crossfader = engine.crossfader.load(std::memory_order_acquire);
        state.mixer.master = engine.master.load(std::memory_order_acquire);
        state.mixer.headphoneLevel = engine.headphoneLevel.load(std::memory_order_acquire);
        for (std::size_t deck = 0; deck < broke::deckCount; ++deck) {
            auto& target = state.decks[deck];
            auto& control = engine.control(deck);
            if (deckFiles[deck] != juce::File{})
                target.path = deckFiles[deck].getFullPathName().toStdString();
            const double position = engine.meter(deck).position.load(std::memory_order_acquire);
            target.positionSeconds = std::isfinite(position) && position >= 0.0 ? position : 0.0;
            target.playbackRate = control.rate.load(std::memory_order_acquire);
            target.trimDb = control.trimDb.load(std::memory_order_acquire);
            target.channelGain = control.gain.load(std::memory_order_acquire);
            target.low = control.low.load(std::memory_order_acquire);
            target.mid = control.mid.load(std::memory_order_acquire);
            target.high = control.high.load(std::memory_order_acquire);
            target.echo = control.echo.load(std::memory_order_acquire);
            target.drive = control.drive.load(std::memory_order_acquire);
            target.headphoneCue = control.headphone.load(std::memory_order_acquire);
            target.wholeTrackLoop = control.loop.load(std::memory_order_acquire);
            target.wasPlaying = control.playing.load(std::memory_order_acquire);
        }
        return state;
    }

    void prepareForSessionRestore(const broke::session::MixerState& mixer) {
        clearSyncFollowers();
        syncMasterDeck.reset();
        for (std::size_t deck = 0; deck < broke::deckCount; ++deck) {
            auto& control = engine.control(deck);
            control.playing.store(false, std::memory_order_release);
            control.reverse.store(false, std::memory_order_release);
            control.slip.store(false, std::memory_order_release);
        }
        const float cross = std::clamp(mixer.crossfader, 0.0f, 1.0f);
        const float masterValue = std::clamp(mixer.master, 0.0f, 1.0f);
        const float cueValue = std::clamp(mixer.headphoneLevel, 0.0f, 1.0f);
        engine.crossfader.store(cross, std::memory_order_release);
        engine.master.store(masterValue, std::memory_order_release);
        engine.headphoneLevel.store(cueValue, std::memory_order_release);
        crossfader.setValue(cross, juce::dontSendNotification);
        master.setValue(masterValue, juce::dontSendNotification);
        headphone.setValue(cueValue, juce::dontSendNotification);
    }

    [[nodiscard]] bool sessionDeckLoading(std::size_t deck) const noexcept {
        return deck < broke::deckCount && loading[deck];
    }

    [[nodiscard]] bool sessionDeckMatches(std::size_t deck, const juce::File& file) const {
        return deck < broke::deckCount
            && deckFiles[deck].getFullPathName() == file.getFullPathName();
    }

    [[nodiscard]] double sessionDeckDuration(std::size_t deck) const noexcept {
        if (deck >= broke::deckCount) return 0.0;
        const double duration = engine.meter(deck).duration.load(std::memory_order_acquire);
        return std::isfinite(duration) && duration > 0.0 ? duration : 0.0;
    }

    [[nodiscard]] bool applyRestoredDeckState(std::size_t deck,
                                              const broke::session::DeckState& state) {
        if (deck >= broke::deckCount) return false;
        const double duration = sessionDeckDuration(deck);
        if (!(duration > 0.0)) return false;
        auto& control = engine.control(deck);
        control.playing.store(false, std::memory_order_release);
        control.reverse.store(false, std::memory_order_release);
        control.slip.store(false, std::memory_order_release);
        control.loop.store(state.wholeTrackLoop, std::memory_order_release);
        control.headphone.store(state.headphoneCue, std::memory_order_release);
        control.rate.store(std::clamp(state.playbackRate, 0.5f, 1.5f), std::memory_order_release);
        control.trimDb.store(std::clamp(state.trimDb, broke::minTrimDb, broke::maxTrimDb),
                             std::memory_order_release);
        control.gain.store(std::clamp(state.channelGain, 0.0f, 1.5f), std::memory_order_release);
        control.low.store(std::clamp(state.low, 0.0f, 2.0f), std::memory_order_release);
        control.mid.store(std::clamp(state.mid, 0.0f, 2.0f), std::memory_order_release);
        control.high.store(std::clamp(state.high, 0.0f, 2.0f), std::memory_order_release);
        control.echo.store(std::clamp(state.echo, 0.0f, 0.7f), std::memory_order_release);
        control.drive.store(std::clamp(state.drive, 0.0f, 6.0f), std::memory_order_release);
        const double position = std::clamp(state.positionSeconds, 0.0, duration);
        control.seek.store(duration > 0.0 ? position / duration : 0.0, std::memory_order_release);
        decks[deck]->syncSessionControls(state);
        decks[deck]->refresh();
        return true;
    }

    void showWorkflowStatus(const juce::String& message) { statusMessage(message); }

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
    [[nodiscard]] bool setSyncLock(std::size_t followerDeck, bool enabled);
    void clearSyncFollowers() noexcept;
    void serviceContinuousSync();
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
    std::array<bool, broke::deckCount> syncFollowers{};
    std::uint32_t syncServiceTick = 0;
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
    CrossfaderSlider crossfader{engine}, master{engine, false}, headphone{engine, false};
    std::atomic<bool> audioReady{false};
    juce::TooltipWindow tooltips{this, 600};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};