// SPDX-License-Identifier: AGPL-3.0-only
#include "MainComponent.h"
#include <algorithm>
#include <cmath>

namespace {
const juce::Colour background{0xff080e1a}, panel{0xff111d30}, blue{0xff3d9bff}, pale{0xffdcecff}, muted{0xff8199b8};
juce::String clockText(double seconds) {
    const int whole = static_cast<int>(std::max(0.0, seconds));
    return juce::String(whole / 60) + ":" + juce::String(whole % 60).paddedLeft('0', 2);
}
juce::String dbfsText(float linear) {
    return std::isfinite(linear) && linear > 0.000001f
        ? juce::String(20.0f * std::log10(linear), 1) + " dBFS"
        : juce::String("— dBFS");
}
void configureLabel(juce::Label& label, const juce::String& value, float size = 13.0f) {
    label.setText(value, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, pale);
    label.setFont(juce::Font(juce::FontOptions(size)));
}
double beatLoopBeatsForId(int id) noexcept {
    constexpr std::array<double, 5> values{1.0, 2.0, 4.0, 8.0, 16.0};
    if (id < 1 || id > static_cast<int>(values.size())) return 4.0;
    return values[static_cast<std::size_t>(id - 1)];
}
int beatLoopIdForBeats(double beats) noexcept {
    constexpr std::array<double, 5> values{1.0, 2.0, 4.0, 8.0, 16.0};
    for (std::size_t i = 0; i < values.size(); ++i)
        if (std::abs(values[i] - beats) < 1.0e-9) return static_cast<int>(i + 1);
    return 3;
}
std::size_t setHotCueCount(const broke::PerformanceDeckOwner::HotCueBank& cues) noexcept {
    return static_cast<std::size_t>(std::count_if(cues.begin(), cues.end(), [](const auto& cue) { return cue.set; }));
}
juce::String deckLetter(std::size_t deck) {
    return juce::String::charToString(static_cast<juce::juce_wchar>('A' + static_cast<int>(deck)));
}
}
juce::String text(const char* english, const char* polish) {
    static const bool usePolish = juce::SystemStats::getUserLanguage().startsWithIgnoreCase("pl");
    return juce::String::fromUTF8(usePolish ? polish : english);
}
void Waveform::setBeatGrid(const broke::BeatGrid& grid, bool manual) {
    beatGrid = grid;
    manualGrid = manual;
    repaint();
}
void Waveform::setDuration(double seconds) noexcept {
    durationSeconds = std::isfinite(seconds) && seconds > 0.0 ? seconds : 0.0;
}
void Waveform::paint(juce::Graphics& g) {
    auto area = getLocalBounds().toFloat();
    g.setColour(background); g.fillRoundedRectangle(area, 7.0f);
    const float middle = area.getCentreY();
    g.setColour(blue.withAlpha(0.18f)); g.drawHorizontalLine(static_cast<int>(middle), 0, area.getWidth());
    if (peaks.empty()) {
        g.setColour(muted); g.setFont(13.0f);
        g.drawFittedText(text("Drop a track here", "Upuść tutaj utwór"), getLocalBounds(), juce::Justification::centred, 1);
        return;
    }
    g.setColour(blue.withAlpha(0.8f));
    for (std::size_t i = 0; i < peaks.size(); ++i) {
        const float x = static_cast<float>(i) * area.getWidth() / static_cast<float>(peaks.size());
        const float h = std::max(0.6f, peaks[i] * (middle - 5));
        g.drawLine(x, middle - h, x, middle + h, 1.0f);
    }

    if (beatGrid.valid() && durationSeconds > 0.0) {
        const double firstBeatValue = beatGrid.beatAtTime(0.0);
        const double lastBeatValue = beatGrid.beatAtTime(durationSeconds);
        if (std::isfinite(firstBeatValue) && std::isfinite(lastBeatValue)) {
            const auto firstBeat = static_cast<long long>(std::floor(firstBeatValue));
            const auto lastBeat = static_cast<long long>(std::ceil(lastBeatValue));
            const auto span = std::max<long long>(0, lastBeat - firstBeat + 1);
            long long stride = 1;
            while (span / stride > 256) stride *= 2;
            for (long long beat = firstBeat; beat <= lastBeat; beat += stride) {
                const double seconds = beatGrid.timeAtBeat(static_cast<double>(beat));
                if (!std::isfinite(seconds) || seconds < 0.0 || seconds > durationSeconds) continue;
                const float x = static_cast<float>(seconds / durationSeconds) * area.getWidth();
                const bool barLine = beat % 4 == 0;
                g.setColour((manualGrid ? pale : muted).withAlpha(barLine ? 0.55f : 0.28f));
                g.drawVerticalLine(static_cast<int>(std::lround(x)), 4.0f, area.getHeight() - 4.0f);
            }
        }
    }

    const float x = juce::jlimit(0.0f, 1.0f, progress) * area.getWidth();
    g.setColour(pale); g.drawLine(x, 3, x, area.getHeight() - 3, 2.0f);
}
void Waveform::mouseDown(const juce::MouseEvent& event) {
    if (!peaks.empty() && onSeek) onSeek(juce::jlimit(0.0, 1.0, static_cast<double>(event.x) / std::max(1, getWidth())));
}
DeckPanel::DeckPanel(broke::Engine& e, std::size_t d) : engine(e), index(d) {
    configureLabel(heading, "DECK " + deckLetter(d) + (d % 2 == 0 ? "  /  LEFT" : "  /  RIGHT"), 16);
    configureLabel(track, text("No track loaded", "Nie wczytano utworu"), 14);
    configureLabel(time, "00:00 / 00:00", 12);
    configureLabel(rhythm, text("BPM —  /  GRID —  /  KEY —", "BPM —  /  SIATKA —  /  TONACJA —"), 11);
    rhythm.setColour(juce::Label::textColourId, muted);
    time.setJustificationType(juce::Justification::centredRight);
    load.setButtonText(text("Load", "Wczytaj"));
    play.setButtonText("PLAY"); rewind.setButtonText("CUE 0");
    loop.setButtonText("LOOP"); beatLoop.setButtonText(text("BEAT LOOP", "PĘTLA BEAT"));
    cue.setButtonText(text("Headphones", "Słuchawki"));
    loop.setClickingTogglesState(true); beatLoop.setClickingTogglesState(true); cue.setClickingTogglesState(true);
    for (int i = 0; i < 5; ++i) {
        constexpr std::array<const char*, 5> labels{"1", "2", "4", "8", "16"};
        beatLoopLength.addItem(labels[static_cast<std::size_t>(i)], i + 1);
        jumpLength.addItem(labels[static_cast<std::size_t>(i)], i + 1);
    }
    beatLoopLength.setSelectedId(3, juce::dontSendNotification);
    jumpLength.setSelectedId(3, juce::dontSendNotification);
    beatLoop.setEnabled(false); beatLoopLength.setEnabled(false);
    jumpBack.setButtonText(text("JUMP -", "SKOK -"));
    jumpForward.setButtonText(text("JUMP +", "SKOK +"));
    reverse.setButtonText("REV");
    slip.setButtonText("SLIP");
    reverse.setClickingTogglesState(true);
    slip.setClickingTogglesState(true);
    reverse.setEnabled(false);
    slip.setEnabled(false);
    syncMaster.setButtonText("MASTER");
    sync.setButtonText("SYNC");
    syncMaster.setClickingTogglesState(true);
    sync.setClickingTogglesState(true);
    jumpBack.setEnabled(false); jumpForward.setEnabled(false); jumpLength.setEnabled(false);
    syncMaster.setEnabled(false); sync.setEnabled(false);
    load.onClick = [this] { if (onBrowse) onBrowse(); };
    play.onClick = [this] {
        auto& p = engine.control(index).playing;
        const bool next = !p.load();
        if (next && onBeforePlay) onBeforePlay();
        p.store(next);
    };
    rewind.onClick = [this] {
        engine.control(index).playing = false;
        engine.control(index).seek = 0.0;
        if (onSeekRequested) onSeekRequested(0.0);
    };
    loop.onClick = [this] {
        const bool enabled = loop.getToggleState();
        if (onWholeTrackLoopRequested) onWholeTrackLoopRequested(enabled);
        else engine.control(index).loop = enabled;
        if (enabled) beatLoop.setToggleState(false, juce::dontSendNotification);
        if (onKeyLockControlChanged) onKeyLockControlChanged();
    };
    beatLoop.onClick = [this] {
        const bool enabled = beatLoop.getToggleState();
        const double beats = beatLoopBeatsForId(beatLoopLength.getSelectedId());
        const bool accepted = onBeatLoopRequested && onBeatLoopRequested(beats, enabled);
        if (!accepted) {
            beatLoop.setToggleState(false, juce::dontSendNotification);
            return;
        }
        if (enabled) loop.setToggleState(false, juce::dontSendNotification);
        if (onKeyLockControlChanged) onKeyLockControlChanged();
    };
    beatLoopLength.onChange = [this] {
        if (!beatLoop.getToggleState()) return;
        const double beats = beatLoopBeatsForId(beatLoopLength.getSelectedId());
        if (onBeatLoopRequested && onBeatLoopRequested(beats, true)) {
            if (onKeyLockControlChanged) onKeyLockControlChanged();
        }
    };
    jumpBack.onClick = [this] {
        if (onBeatJumpRequested)
            onBeatJumpRequested(-beatLoopBeatsForId(jumpLength.getSelectedId()));
    };
    jumpForward.onClick = [this] {
        if (onBeatJumpRequested)
            onBeatJumpRequested(beatLoopBeatsForId(jumpLength.getSelectedId()));
    };
    const auto requestReverseSlip = [this] {
        const bool reverseRequested = reverse.getToggleState();
        const bool slipRequested = slip.getToggleState();
        const auto mode = reverseRequested
            ? (slipRequested ? broke::PerformanceDeckOwner::ReverseSlipMode::slipReverse
                             : broke::PerformanceDeckOwner::ReverseSlipMode::reverse)
            : (slipRequested ? broke::PerformanceDeckOwner::ReverseSlipMode::slipArmed
                             : broke::PerformanceDeckOwner::ReverseSlipMode::forward);
        const bool accepted = onReverseSlipModeRequested && onReverseSlipModeRequested(mode);
        if (!accepted) {
            reverse.setToggleState(engine.control(index).reverse.load(std::memory_order_acquire), juce::dontSendNotification);
            slip.setToggleState(engine.control(index).slip.load(std::memory_order_acquire), juce::dontSendNotification);
        }
    };
    reverse.onClick = requestReverseSlip;
    slip.onClick = requestReverseSlip;
    syncMaster.onClick = [this] {
        if (onSyncMasterRequested) onSyncMasterRequested(syncMaster.getToggleState());
    };
    sync.onClick = [this] {
        const bool requested = sync.getToggleState();
        const bool accepted = onSyncRequested && onSyncRequested(requested);
        if (!accepted) sync.setToggleState(false, juce::dontSendNotification);
    };
    cue.onClick = [this] { engine.control(index).headphone = cue.getToggleState(); };
    waveform.onSeek = [this](double position) {
        engine.control(index).seek = position;
        if (onSeekRequested) onSeekRequested(position);
    };
    for (juce::Component* child : std::array<juce::Component*, 10>{&heading, &track, &time, &rhythm, &waveform, &load, &play, &rewind, &loop, &cue}) addAndMakeVisible(child);
    addAndMakeVisible(beatLoop); addAndMakeVisible(beatLoopLength);
    addAndMakeVisible(jumpBack); addAndMakeVisible(jumpLength); addAndMakeVisible(jumpForward);
    addAndMakeVisible(reverse); addAndMakeVisible(slip);
    addAndMakeVisible(syncMaster); addAndMakeVisible(sync);

    for (std::size_t i = 0; i < hotCuePads.size(); ++i) {
        auto& pad = hotCuePads[i];
        pad.setButtonText("HC" + juce::String(static_cast<int>(i + 1)));
        pad.setEnabled(false);
        pad.onClick = [this, i] {
            if (onHotCueRequested)
                onHotCueRequested(i, juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown());
        };
        pad.setTooltip(text("Hot cue: click an empty pad to store the current position; click a set pad to jump. Hold Shift while clicking to clear it. Positions are quantized only when a reviewed beat grid is available.",
                            "Hot cue: kliknij pusty pad, aby zapisać pozycję; kliknij zapisany, aby skoczyć. Shift+klik usuwa pad. Pozycja jest kwantyzowana tylko przy dostępnej zweryfikowanej siatce rytmu."));
        addAndMakeVisible(pad);
    }

    const std::array<juce::String, 8> names {
        "TRIM dB", text("Fader", "Fader"), text("Rate %", "Tempo %"),
        "LOW", "MID", "HIGH", "ECHO", "DRIVE"
    };
    for (std::size_t i = 0; i < knobs.size(); ++i) {
        auto& knob = knobs[i];
        knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
        double minimum = 0.0, maximum = 2.0, initial = 0.0;
        switch (i) {
            case 0: minimum = broke::minTrimDb; maximum = broke::maxTrimDb; initial = 0.0; break;
            case 1: minimum = 0.0; maximum = 1.5; initial = 0.7; break;
            case 2: minimum = -20.0; maximum = 20.0; initial = 0.0; break;
            case 3:
            case 4:
            case 5: minimum = 0.0; maximum = 2.0; initial = 1.0; break;
            case 6: minimum = 0.0; maximum = 0.7; initial = 0.0; break;
            case 7: minimum = 0.0; maximum = 6.0; initial = 0.0; break;
            default: break;
        }
        knob.setRange(minimum, maximum, 0.01);
        knob.setValue(initial); knob.setDoubleClickReturnValue(true, initial);
        knob.onValueChange = [this, i] {
            const auto value = static_cast<float>(knobs[i].getValue());
            auto& c = engine.control(index);
            switch (i) {
                case 0: c.trimDb = value; break;
                case 1: c.gain = value; break;
                case 2: c.rate = 1.0f + value / 100.0f; break;
                case 3: c.low = value; break;
                case 4: c.mid = value; break;
                case 5: c.high = value; break;
                case 6: c.echo = value; break;
                case 7: c.drive = value; break;
                default: break;
            }
            if (i == 2 && onKeyLockControlChanged) onKeyLockControlChanged();
        };
        configureLabel(knobNames[i], names[i], 11);
        knobNames[i].setJustificationType(juce::Justification::centred);
        knob.setName(names[i]); addAndMakeVisible(knob); addAndMakeVisible(knobNames[i]);
    }

    configureLabel(gridZeroLabel, text("GRID ZERO", "ZERO SIATKI"), 10);
    configureLabel(gridBpmLabel, text("GRID BPM", "BPM SIATKI"), 10);
    gridZeroLabel.setJustificationType(juce::Justification::centred);
    gridBpmLabel.setJustificationType(juce::Justification::centred);
    for (auto* slider : {&gridZero, &gridBpm}) {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 76, 20);
        slider->setChangeNotificationOnlyOnRelease(true);
        slider->setEnabled(false);
        addAndMakeVisible(slider);
    }
    gridZero.setRange(0.0, 24.0 * 60.0 * 60.0, 0.001);
    gridZero.setNumDecimalPlacesToDisplay(3);
    gridZero.setTextValueSuffix(" s");
    gridBpm.setRange(30.0, 300.0, 0.01);
    gridBpm.setNumDecimalPlacesToDisplay(2);
    gridBpm.setTextValueSuffix(" BPM");
    const auto commitGridEdit = [this] {
        if (activeBeatGrid.valid() && onGridEdit)
            onGridEdit(gridZero.getValue(), gridBpm.getValue());
    };
    gridZero.onValueChange = commitGridEdit;
    gridBpm.onValueChange = commitGridEdit;
    gridReset.setButtonText(text("Reset grid", "Reset siatki"));
    gridReset.setEnabled(false);
    gridReset.onClick = [this] { if (onGridReset) onGridReset(); };
    tempoMap.setButtonText(text("Tempo map", "Mapa tempa"));
    tempoMap.setEnabled(false);
    tempoMap.onClick = [this] { if (onTempoMapRequested) onTempoMapRequested(); };
    addAndMakeVisible(gridZeroLabel); addAndMakeVisible(gridBpmLabel);
    addAndMakeVisible(gridReset); addAndMakeVisible(tempoMap);

    const auto gridTooltip = text(
        "Manual base-grid correction. Beat zero shifts all preserved tempo-change boundaries; GRID BPM edits segment 0 only. Changes are stored locally outside the audio callback.",
        "Ręczna korekta podstawy siatki. Zero przesuwa wszystkie zachowane zmiany tempa; BPM SIATKI edytuje tylko segment 0. Zmiany są zapisywane lokalnie poza callbackiem audio.");
    gridZero.setTooltip(gridTooltip);
    gridBpm.setTooltip(gridTooltip);
    gridReset.setTooltip(text("Erase the local manual override and return to the detected grid for this file.",
                              "Usuń lokalną ręczną korektę i wróć do wykrytej siatki dla tego pliku."));
    tempoMap.setTooltip(text(
        "Open the reviewed variable-tempo map editor. Add/move/remove later boundaries and edit segment BPM without doing file I/O in the audio callback.",
        "Otwórz edytor zweryfikowanej mapy zmiennego tempa. Dodawaj/przesuwaj/usuwaj późniejsze granice i zmieniaj BPM segmentów bez I/O w callbacku audio."));
    knobs[0].setTooltip(text("Input trim before EQ, FX and headphone cue. Keep the deck peak below 0 dBFS before using the channel fader.",
                             "Trim wejściowy przed EQ, efektami i odsłuchem. Utrzymuj szczyt decku poniżej 0 dBFS przed użyciem fadera kanału."));
    knobs[1].setTooltip(text("Post-FX channel fader. Headphone cue stays pre-fader but follows input trim and EQ/FX.",
                             "Fader kanału po efektach. Odsłuch pozostaje pre-fader, ale reaguje na trim wejściowy oraz EQ/efekty."));
    knobs[2].setTooltip(text("Playback rate changes pitch unless the opt-in research key-lock path is active. Sync can update this control without feeding a second rate command back into the Engine.", "Zmiana tempa zmienia tonację, chyba że aktywna jest testowa ścieżka key lock. Sync może zaktualizować tę kontrolkę bez wysyłania drugiej komendy tempa do silnika."));
    knobs[6].setTooltip(text("Fixed 250 ms echo; not beat-synchronized yet.", "Echo 250 ms; jeszcze bez synchronizacji do BPM."));
    loop.setTooltip(text("Loops the whole track. Beat-length looping is a separate reviewed-grid control.", "Zapętla cały utwór. Pętla beatowa ma osobną kontrolkę opartą o zweryfikowaną siatkę."));
    beatLoop.setTooltip(text("Arm/disarm a reviewed-grid musical loop at the current transport position.", "Włącz/wyłącz muzyczną pętlę z siatki rytmu w bieżącej pozycji."));
    beatLoopLength.setTooltip(text("Beat-loop length: 1, 2, 4, 8 or 16 beats.", "Długość pętli: 1, 2, 4, 8 lub 16 beatów."));
    jumpBack.setTooltip(text("Jump backward by the selected number of beats while preserving fractional beat phase. Requires a reviewed grid.", "Skocz wstecz o wybraną liczbę beatów z zachowaniem fazy. Wymaga zweryfikowanej siatki."));
    jumpForward.setTooltip(text("Jump forward by the selected number of beats while preserving fractional beat phase. Requires a reviewed grid.", "Skocz do przodu o wybraną liczbę beatów z zachowaniem fazy. Wymaga zweryfikowanej siatki."));
    jumpLength.setTooltip(text("Beat Jump distance: 1, 2, 4, 8 or 16 beats.", "Dystans Beat Jump: 1, 2, 4, 8 lub 16 beatów."));
    reverse.setTooltip(text("Reverse audible playback. Whole-track LOOP remains compatible; reviewed Beat Loop and incompatible external renderers reject Reverse fail-closed.",
                            "Odwróć słyszalne odtwarzanie. LOOP całego utworu pozostaje zgodny; Pętla Beat i niezgodne renderery bezpiecznie odrzucają Reverse."));
    slip.setTooltip(text("Arm Slip. With REV active, the audible cursor moves backward while the hidden transport keeps moving forward; releasing REV rejoins the hidden timeline.",
                         "Uzbrój Slip. Przy aktywnym REV słyszalny kursor cofa się, a ukryty transport idzie dalej do przodu; wyłączenie REV wraca do ukrytej osi czasu."));
    syncMaster.setTooltip(text("Select this deck as the explicit Sync master. Only one reviewed-grid deck is master at a time.", "Ustaw ten deck jako jawny master Sync. Jednocześnie masterem może być tylko jeden deck ze zweryfikowaną siatką."));
    sync.setTooltip(text("Toggle reviewed-grid continuous Sync to the selected master. Initial alignment is bounded; maintenance runs on the message thread, updates tempo only when needed, and phase-corrects only outside a small deadband. Beat Loop, Reverse/Slip and active Jog suspend or release the lock fail-closed.",
                         "Włącz/wyłącz ciągły Sync do wybranego mastera na zweryfikowanej siatce. Początkowe wyrównanie jest ograniczone; podtrzymanie działa w wątku interfejsu, zmienia tempo tylko gdy trzeba i koryguje fazę poza małą strefą martwą. Pętla Beat, Reverse/Slip i aktywny Jog bezpiecznie zawieszają lub zwalniają blokadę."));
    cue.setTooltip(text("Cue uses outputs 3/4 only. Enable four outputs in Audio settings.", "Odsłuch używa tylko wyjść 3/4. Włącz cztery wyjścia w ustawieniach audio."));
}
void DeckPanel::setLoading(bool isLoading) {
    load.setEnabled(!isLoading); play.setEnabled(!isLoading);
    if (isLoading) track.setText(text("Decoding track...", "Dekodowanie utworu..."), juce::dontSendNotification);
}
void DeckPanel::setTrack(const juce::String& name, std::vector<float> peaks) {
    track.setText(name, juce::dontSendNotification); track.setTooltip(name);
    waveform.peaks = std::move(peaks); waveform.repaint();
}
void DeckPanel::setRhythmPending() {
    rhythmReady = false;
    activeBeatGrid = {};
    manualBeatGrid = false;
    gridZero.setEnabled(false); gridBpm.setEnabled(false); gridReset.setEnabled(false); tempoMap.setEnabled(false);
    broke::PerformanceDeckOwner::HotCueBank emptyCues{};
    setPerformanceState(false, false, 0.0, emptyCues, false, false, false, false);
    waveform.setBeatGrid({}, false);
    rhythm.setText(text("Analyzing BPM / beat grid / key…", "Analiza BPM / siatki rytmu / tonacji…"), juce::dontSendNotification);
    rhythm.setTooltip(text("Offline musical analysis runs in a background worker and never in the audio callback.",
                           "Analiza muzyczna offline działa w tle i nigdy w callbacku audio."));
}
void DeckPanel::setRhythmAnalysis(const TrackRhythmAnalysis& analysis,
                                  const broke::BeatGrid& grid,
                                  bool manual) {
    rhythmAnalysis = analysis;
    rhythmReady = true;
    activeBeatGrid = grid;
    manualBeatGrid = manual;
    refreshRhythmDisplay();
}
void DeckPanel::setBeatGrid(const broke::BeatGrid& grid, bool manual) {
    activeBeatGrid = grid;
    manualBeatGrid = manual;
    if (rhythmReady) refreshRhythmDisplay();
    else waveform.setBeatGrid(grid, manual);
}
void DeckPanel::setPerformanceState(bool gridAvailable, bool beatLoopIsActive, double beatLoopBeats,
                                    const broke::PerformanceDeckOwner::HotCueBank& hotCues,
                                    bool trackReady, bool isSyncMaster, bool syncAvailable, bool syncLocked) {
    beatLoop.setEnabled(gridAvailable);
    beatLoopLength.setEnabled(gridAvailable);
    beatLoop.setToggleState(beatLoopIsActive, juce::dontSendNotification);
    if (beatLoopIsActive)
        beatLoopLength.setSelectedId(beatLoopIdForBeats(beatLoopBeats), juce::dontSendNotification);
    jumpBack.setEnabled(gridAvailable && trackReady);
    jumpForward.setEnabled(gridAvailable && trackReady);
    jumpLength.setEnabled(gridAvailable && trackReady);
    reverse.setEnabled(trackReady && !beatLoopIsActive);
    slip.setEnabled(trackReady && !beatLoopIsActive);
    syncMaster.setEnabled(gridAvailable && trackReady);
    syncMaster.setToggleState(isSyncMaster, juce::dontSendNotification);
    sync.setEnabled(syncAvailable || syncLocked);
    sync.setToggleState(syncLocked, juce::dontSendNotification);
    for (std::size_t i = 0; i < hotCuePads.size(); ++i) {
        auto& pad = hotCuePads[i];
        pad.setEnabled(trackReady);
        pad.setColour(juce::TextButton::buttonColourId,
                      hotCues[i].set ? blue.withAlpha(0.72f) : juce::Colour(0xff1b2d46));
        pad.setColour(juce::TextButton::textColourOffId, hotCues[i].set ? juce::Colours::white : pale);
    }
}
void DeckPanel::refreshRhythmDisplay() {
    juce::String summary;
    if (activeBeatGrid.valid() && !activeBeatGrid.segments().empty()) {
        summary = "BPM " + juce::String(activeBeatGrid.segments().front().bpm, 1)
            + (manualBeatGrid ? "  /  GRID* +" : "  /  GRID +")
            + juce::String(activeBeatGrid.beatZeroSeconds(), 3) + " s";
    } else {
        summary = text("BPM —  /  GRID —", "BPM —  /  SIATKA —");
    }

    if (rhythmAnalysis.key.valid) {
        const auto mode = rhythmAnalysis.key.mode == broke::KeyMode::major
            ? text("MAJ", "DUR") : text("MIN", "MOLL");
        summary += text("  /  KEY ", "  /  TONACJA ")
            + juce::String(broke::keyName(rhythmAnalysis.key.tonic)) + " " + mode;
    } else {
        summary += text("  /  KEY —", "  /  TONACJA —");
    }
    rhythm.setText(summary, juce::dontSendNotification);

    auto tooltip = manualBeatGrid
        ? text("Manual beat-grid override is active for this exact local file. Key metadata remains the offline estimate.",
               "Aktywna jest ręczna korekta siatki dla dokładnie tego lokalnego pliku. Tonacja pozostaje wynikiem analizy offline.")
        : text("Offline tempo/grid and musical-key estimates. Beat Loop, Beat Jump and one-shot Sync use only a reviewed grid; current detector accuracy evidence is still deterministic/synthetic.",
               "Szacunki offline tempa/siatki i tonacji. Beat Loop, Beat Jump i jednorazowy Sync używają tylko zweryfikowanej siatki; obecna walidacja detektora nadal jest deterministyczna/syntetyczna.");
    if (rhythmAnalysis.beat.valid) {
        const int confidence = juce::jlimit(0, 100,
            static_cast<int>(std::lround(rhythmAnalysis.beat.confidence * 100.0)));
        tooltip += text(" Tempo confidence: ", " Pewność tempa: ") + juce::String(confidence) + "%";
    } else if (rhythmAnalysis.beatError.isNotEmpty()) {
        tooltip += " " + rhythmAnalysis.beatError;
    }
    if (rhythmAnalysis.key.valid) {
        const int confidence = juce::jlimit(0, 100,
            static_cast<int>(std::lround(rhythmAnalysis.key.confidence * 100.0)));
        tooltip += text(" Key confidence: ", " Pewność tonacji: ") + juce::String(confidence) + "%";
    } else if (rhythmAnalysis.keyError.isNotEmpty()) {
        tooltip += " " + rhythmAnalysis.keyError;
    }
    if (rhythmAnalysis.error.isNotEmpty()) tooltip += " " + rhythmAnalysis.error;
    if (rhythmAnalysis.cacheHit)
        tooltip += text(" Loaded from local analysis cache.", " Wczytano z lokalnego cache analizy.");
    rhythm.setTooltip(tooltip);

    const bool editable = activeBeatGrid.valid() && !activeBeatGrid.segments().empty();
    gridZero.setEnabled(editable);
    gridBpm.setEnabled(editable);
    gridReset.setEnabled(editable && manualBeatGrid);
    tempoMap.setEnabled(editable);
    if (editable) {
        gridZero.setValue(activeBeatGrid.beatZeroSeconds(), juce::dontSendNotification);
        gridBpm.setValue(activeBeatGrid.segments().front().bpm, juce::dontSendNotification);
    }
    waveform.setBeatGrid(activeBeatGrid, manualBeatGrid);
}
void DeckPanel::refresh() {
    const auto& meter = engine.meter(index);
    const auto duration = meter.duration.load(std::memory_order_acquire);
    const auto position = meter.position.load(std::memory_order_acquire);
    const auto audiblePosition = meter.audiblePosition.load(std::memory_order_acquire);
    const bool reverseActive = engine.control(index).reverse.load(std::memory_order_acquire);
    const bool slipActive = engine.control(index).slip.load(std::memory_order_acquire);
    const bool splitCursor = reverseActive && slipActive
        && std::isfinite(position) && std::isfinite(audiblePosition)
        && std::abs(position - audiblePosition) > 0.02;

    reverse.setToggleState(reverseActive, juce::dontSendNotification);
    slip.setToggleState(slipActive, juce::dontSendNotification);
    if (splitCursor) {
        time.setText(text("AUD ", "SŁYSZ ") + clockText(audiblePosition)
                     + text("  /  HIDDEN ", "  /  UKRYTY ") + clockText(position),
                     juce::dontSendNotification);
        time.setTooltip(text("Slip split transport: AUD is the source position currently heard; HIDDEN is the uninterrupted transport position that playback rejoins when Reverse is released.",
                             "Transport Slip jest rozdzielony: SŁYSZ to aktualnie słyszana pozycja źródła; UKRYTY to nieprzerwana pozycja transportu, do której odtwarzanie wróci po wyłączeniu Reverse."));
    } else {
        time.setText(clockText(position) + " / " + clockText(duration), juce::dontSendNotification);
        time.setTooltip({});
    }
    const auto preFaderPeak = meter.preFaderPeak.load(std::memory_order_relaxed);
    heading.setText("DECK " + deckLetter(index) + (index % 2 == 0 ? "  /  LEFT" : "  /  RIGHT")
                    + "  |  PK " + dbfsText(preFaderPeak)
                    + (meter.overloaded.load(std::memory_order_relaxed) ? "  OVR" : ""),
                    juce::dontSendNotification);
    heading.setColour(juce::Label::textColourId,
                      meter.overloaded.load(std::memory_order_relaxed) ? blue.brighter(0.45f) : pale);
    const double waveformPosition = splitCursor ? audiblePosition : position;
    waveform.progress = duration > 0 ? static_cast<float>(waveformPosition / duration) : 0;
    waveform.setDuration(duration);
    waveform.repaint();
    play.setButtonText(engine.control(index).playing.load() ? "PAUSE" : "PLAY");

    const auto& control = engine.control(index);
    const double trimDb = static_cast<double>(control.trimDb.load(std::memory_order_relaxed));
    if (std::isfinite(trimDb) && std::abs(knobs[0].getValue() - trimDb) > 0.005)
        knobs[0].setValue(juce::jlimit<double>(broke::minTrimDb, broke::maxTrimDb, trimDb), juce::dontSendNotification);
    const double channelFader = static_cast<double>(control.gain.load(std::memory_order_relaxed));
    if (std::isfinite(channelFader) && std::abs(knobs[1].getValue() - channelFader) > 0.005)
        knobs[1].setValue(juce::jlimit(0.0, 1.5, channelFader), juce::dontSendNotification);
    const double rate = static_cast<double>(control.rate.load(std::memory_order_relaxed));
    if (std::isfinite(rate)) {
        const double percent = juce::jlimit(-20.0, 20.0, (rate - 1.0) * 100.0);
        if (std::abs(knobs[2].getValue() - percent) > 0.005)
            knobs[2].setValue(percent, juce::dontSendNotification);
    }
}
void DeckPanel::paint(juce::Graphics& g) {
    g.setColour(panel); g.fillRoundedRectangle(getLocalBounds().toFloat(), 12.0f);
    g.setColour(blue.withAlpha(0.25f)); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 12, 1);
}
void DeckPanel::resized() {
    auto area = getLocalBounds().reduced(14);
    auto top = area.removeFromTop(24); time.setBounds(top.removeFromRight(250)); heading.setBounds(top);
    track.setBounds(area.removeFromTop(24));
    rhythm.setBounds(area.removeFromTop(18)); area.removeFromTop(4);
    waveform.setBounds(area.removeFromTop(std::max(42, area.getHeight() - 278)));
    area.removeFromTop(6);
    auto buttons = area.removeFromTop(30);
    std::array<juce::Component*, 7> list {&load, &play, &rewind, &loop, &beatLoop, &beatLoopLength, &cue};
    const int buttonWidth = buttons.getWidth() / static_cast<int>(list.size());
    for (auto* component : list) component->setBounds(buttons.removeFromLeft(buttonWidth).reduced(2, 0));
    area.removeFromTop(4);
    auto hotCueArea = area.removeFromTop(28);
    const int hotCueWidth = hotCueArea.getWidth() / static_cast<int>(hotCuePads.size());
    for (auto& pad : hotCuePads) pad.setBounds(hotCueArea.removeFromLeft(hotCueWidth).reduced(2, 0));
    area.removeFromTop(4);
    auto performanceArea = area.removeFromTop(28);
    std::array<juce::Component*, 7> performanceControls {&jumpBack, &jumpLength, &jumpForward, &reverse, &slip, &syncMaster, &sync};
    const int performanceWidth = performanceArea.getWidth() / static_cast<int>(performanceControls.size());
    for (auto* component : performanceControls)
        component->setBounds(performanceArea.removeFromLeft(performanceWidth).reduced(2, 0));
    area.removeFromTop(4);
    auto gridArea = area.removeFromTop(44);
    auto actions = gridArea.removeFromRight(188);
    gridReset.setBounds(actions.removeFromRight(92).reduced(2, 7));
    tempoMap.setBounds(actions.reduced(2, 7));
    auto zeroArea = gridArea.removeFromLeft(gridArea.getWidth() / 2);
    gridZeroLabel.setBounds(zeroArea.removeFromTop(14)); gridZero.setBounds(zeroArea);
    gridBpmLabel.setBounds(gridArea.removeFromTop(14)); gridBpm.setBounds(gridArea);
    area.removeFromTop(4);
    const int knobWidth = area.getWidth() / static_cast<int>(knobs.size());
    for (std::size_t i = 0; i < knobs.size(); ++i) {
        auto slot = area.removeFromLeft(knobWidth); knobNames[i].setBounds(slot.removeFromTop(18)); knobs[i].setBounds(slot);
    }
}
bool DeckPanel::isInterestedInFileDrag(const juce::StringArray& files) { return files.size() == 1; }
void DeckPanel::filesDropped(const juce::StringArray& files, int, int) { if (files.size() == 1 && onDrop) onDrop(juce::File(files[0])); }

MainComponent::MainComponent(bool openAudio, bool enableKeyLockResearch)
    : author("by Swir", juce::URL("https://github.com/Swir")) {
    theme.setColour(juce::ResizableWindow::backgroundColourId, background);
    theme.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1b2d46));
    theme.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff2163a3));
    theme.setColour(juce::TextButton::textColourOffId, pale);
    theme.setColour(juce::TextButton::textColourOnId, pale);
    theme.setColour(juce::Slider::rotarySliderFillColourId, blue);
    theme.setColour(juce::Slider::rotarySliderOutlineColourId, background);
    theme.setColour(juce::Slider::thumbColourId, blue);
    theme.setColour(juce::Slider::textBoxTextColourId, pale);
    theme.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setLookAndFeel(&theme);
    configureLabel(title, "BrokeDJ", 32);
    configureLabel(subtitle, text("ZERO COST. FULL CONTROL.  /  DEVELOPMENT BUILD 0.1.0", "ZERO KOSZTÓW. PEŁNA KONTROLA.  /  WERSJA ROZWOJOWA 0.1.0"), 11);
    configureLabel(status, text("Local files only. Development build — not yet live-performance qualified.", "Pliki lokalne. Wersja rozwojowa — jeszcze nieprzetestowana do występów na żywo."), 12);
    configureLabel(crossLabel, "A + C       CROSSFADER       B + D", 12);
    configureLabel(masterLabel, "MASTER", 12); configureLabel(cueLabel, text("HEADPHONES", "SŁUCHAWKI"), 12);
    configureLabel(meterLabel, "MASTER: —", 12);
    settings.setButtonText(text("Audio settings", "Ustawienia audio"));
    settings.onClick = [this] { showAudioSettings(); };
    for (juce::Component* c : std::array<juce::Component*, 12>{&title, &subtitle, &status, &crossLabel, &masterLabel, &cueLabel, &meterLabel, &settings, &author, &crossfader, &master, &headphone}) addAndMakeVisible(c);
    for (auto* slider : {&crossfader, &master, &headphone}) {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 22);
        slider->setRange(0, 1, 0.001); slider->setValue(0.5);
        slider->setDoubleClickReturnValue(true, 0.5);
    }
    crossfader.onValueChange = [this] { engine.crossfader = static_cast<float>(crossfader.getValue()); };
    master.onValueChange = [this] { engine.master = static_cast<float>(master.getValue()); };
    headphone.onValueChange = [this] { engine.headphoneLevel = static_cast<float>(headphone.getValue()); };
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
    keyLockResearchEnabled = enableKeyLockResearch;
#else
    juce::ignoreUnused(enableKeyLockResearch);
#endif
    for (std::size_t i = 0; i < decks.size(); ++i) {
        performanceDecks[i] = std::make_unique<broke::PerformanceDeckOwner>(engine, i);
        tempoSegmentEditors[i] = std::make_unique<broke::TempoSegmentEditorModel>(*performanceDecks[i]);
        decks[i] = std::make_unique<DeckPanel>(engine, i);
        decks[i]->onBrowse = [this, i] { browse(i); };
        decks[i]->onDrop = [this, i](const juce::File& file) { load(i, file); };
        decks[i]->onGridEdit = [this, i](double beatZero, double bpm) {
            applyBeatGridEdit(i, beatZero, bpm);
        };
        decks[i]->onGridReset = [this, i] { resetBeatGridEdit(i); };
        decks[i]->onTempoMapRequested = [this, i] { showTempoSegmentEditor(i); };
        decks[i]->onWholeTrackLoopRequested = [this, i](bool enabled) { setWholeTrackLoop(i, enabled); };
        decks[i]->onBeatLoopRequested = [this, i](double beats, bool enabled) {
            return setBeatLoop(i, beats, enabled);
        };
        decks[i]->onHotCueRequested = [this, i](std::size_t slot, bool clear) {
            handleHotCue(i, slot, clear);
        };
        decks[i]->onBeatJumpRequested = [this, i](double beats) {
            static_cast<void>(jumpBeats(i, beats));
        };
        decks[i]->onReverseSlipModeRequested = [this, i](broke::PerformanceDeckOwner::ReverseSlipMode mode) {
            return setReverseSlipMode(i, mode);
        };
        decks[i]->onSyncMasterRequested = [this, i](bool enabled) { setSyncMaster(i, enabled); };
        decks[i]->onSyncRequested = [this, i](bool enabled) { return setSyncLock(i, enabled); };
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
        static_cast<void>(keyLockLifecycle.setEnabled(i, keyLockResearchEnabled));
        decks[i]->onBeforePlay = [this, i] {
            if (keyLockResearchEnabled) serviceKeyLockDeck(i, false);
        };
#endif
        // A manual follower rate/loop action takes transport ownership from Sync.
        // Master rate changes remain legal: followers deliberately re-evaluate the
        // master's effective tempo on the next bounded maintenance tick.
        decks[i]->onKeyLockControlChanged = [this, i] {
            syncFollowers[i] = false;
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
            if (keyLockResearchEnabled) keyLockLifecycle.noteTransportControlChanged(i);
#endif
        };
        // Explicit cursor jumps are stronger than a phase-maintenance lock. A
        // follower jump releases only that follower; a master jump releases all
        // followers so no deck receives a delayed corrective seek after the DJ
        // intentionally moved the reference timeline.
        decks[i]->onSeekRequested = [this, i](double position) {
            if (syncMasterDeck && *syncMasterDeck == i) clearSyncFollowers();
            else syncFollowers[i] = false;
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
            if (keyLockResearchEnabled) keyLockLifecycle.noteSeekNormalized(i, position);
#else
            static_cast<void>(position);
#endif
        };
        addAndMakeVisible(*decks[i]);
    }
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
    if (keyLockResearchEnabled) {
        status.setText(text("Developer key-lock research path enabled; live control changes fail closed to normal playback.",
                            "Włączono testową ścieżkę key lock; zmiany podczas odtwarzania wracają bezpiecznie do zwykłego playbacku."),
                       juce::dontSendNotification);
    }
#endif
    setSize(1280, 860);
    if (openAudio) setAudioChannels(0, 2);
    startTimerHz(25);
}
MainComponent::~MainComponent() {
    stopTimer(); cancelled->store(true);
    for (auto& stop : analysisCancelled) if (stop) stop->store(true);
    if (tempoSegmentDialog) delete tempoSegmentDialog.getComponent();
    if (audioSettings) delete audioSettings.getComponent();
    chooser.reset(); shutdownAudio();
    loaders.removeAllJobs(true, -1);
    analyzers.removeAllJobs(true, -1);
    engine.collectRetired(); setLookAndFeel(nullptr);
}
void MainComponent::prepareToPlay(int, double rate) {
    audioReady = false;
    try {
        engine.prepare(rate);
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
        if (keyLockResearchEnabled
            && !keyLockLifecycle.configureAudioStopped(rate, engine.preparedMaxAudioBlockFrames())) {
            keyLockResearchEnabled = false;
        }
#endif
        audioReady = true;
    } catch (...) { /* UI timer reports unavailable audio. */ }
}
void MainComponent::releaseResources() {
    audioReady = false;
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
    if (keyLockLifecycle.configured()) keyLockLifecycle.releaseAudioStopped();
#endif
}
void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& info) {
    juce::ScopedNoDenormals noDenormals;
    if (!info.buffer || !audioReady.load()) { info.clearActiveBufferRegion(); return; }
    std::array<float*, 64> channels{};
    const int count = std::min(64, info.buffer->getNumChannels());
    for (int c = 0; c < count; ++c) channels[static_cast<std::size_t>(c)] = info.buffer->getWritePointer(c, info.startSample);
    engine.process(channels.data(), count, info.numSamples);
    for (int c = count; c < info.buffer->getNumChannels(); ++c) info.buffer->clear(c, info.startSample, info.numSamples);
}
void MainComponent::paint(juce::Graphics& g) { g.fillAll(background); }
void MainComponent::resized() {
    auto area = getLocalBounds().reduced(20);
    auto top = area.removeFromTop(70);
    settings.setBounds(top.removeFromRight(165).withHeight(34));
    title.setBounds(top.removeFromTop(38)); subtitle.setBounds(top.removeFromTop(24));
    auto footer = area.removeFromBottom(26); author.setBounds(footer.removeFromRight(80)); status.setBounds(footer);
    auto mixer = area.removeFromBottom(86).reduced(0, 8);
    auto cross = mixer.removeFromLeft(mixer.getWidth() / 2).reduced(8, 0);
    crossLabel.setBounds(cross.removeFromTop(20)); crossfader.setBounds(cross.removeFromTop(30)); meterLabel.setBounds(cross);
    auto main = mixer.removeFromLeft(mixer.getWidth() / 2).reduced(8, 0);
    masterLabel.setBounds(main.removeFromTop(20)); master.setBounds(main.removeFromTop(30));
    mixer.reduce(8, 0); cueLabel.setBounds(mixer.removeFromTop(20)); headphone.setBounds(mixer.removeFromTop(30));
    const int rowHeight = (area.getHeight() - 12) / 2;
    for (int row = 0; row < 2; ++row) {
        auto strip = area.removeFromTop(rowHeight);
        const int width = (strip.getWidth() - 12) / 2;
        decks[static_cast<std::size_t>(row * 2)]->setBounds(strip.removeFromLeft(width));
        strip.removeFromLeft(12); decks[static_cast<std::size_t>(row * 2 + 1)]->setBounds(strip);
        area.removeFromTop(12);
    }
}
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
void MainComponent::serviceKeyLockDeck(std::size_t deck, bool playing) {
    if (!keyLockResearchEnabled || !keyLockLifecycle.configured() || deck >= broke::deckCount) return;
    auto& control = engine.control(deck);
    static_cast<void>(keyLockLifecycle.service(
        deck,
        engine.meter(deck).position.load(std::memory_order_relaxed),
        control.loop.load(std::memory_order_relaxed),
        static_cast<double>(control.rate.load(std::memory_order_relaxed)),
        playing));
}
#endif
void MainComponent::timerCallback() {
    engine.collectRetired();
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
    if (keyLockResearchEnabled && keyLockLifecycle.configured()) {
        for (std::size_t deck = 0; deck < broke::deckCount; ++deck)
            serviceKeyLockDeck(deck, engine.control(deck).playing.load(std::memory_order_relaxed));
    }
#endif
    if (++syncServiceTick >= 5) {
        syncServiceTick = 0;
        serviceContinuousSync();
    }
    bool syncMasterReady = false;
    if (syncMasterDeck && *syncMasterDeck < broke::deckCount && performanceDecks[*syncMasterDeck]) {
        const auto masterDuration = engine.meter(*syncMasterDeck).duration.load(std::memory_order_acquire);
        syncMasterReady = performanceDecks[*syncMasterDeck]->hasReviewedGrid()
            && std::isfinite(masterDuration) && masterDuration > 0.0;
    }
    for (std::size_t i = 0; i < decks.size(); ++i) {
        decks[i]->refresh();
        if (performanceDecks[i]) {
            const auto duration = engine.meter(i).duration.load(std::memory_order_acquire);
            const bool trackReady = std::isfinite(duration) && duration > 0.0;
            const bool isMaster = syncMasterDeck && *syncMasterDeck == i;
            const bool syncAvailable = syncMasterReady && !isMaster
                && performanceDecks[i]->hasReviewedGrid() && trackReady;
            decks[i]->setPerformanceState(performanceDecks[i]->hasReviewedGrid(),
                                          performanceDecks[i]->beatLoopActive(),
                                          performanceDecks[i]->beatLoopLength(),
                                          performanceDecks[i]->hotCueBank(),
                                          trackReady, isMaster, syncAvailable, syncFollowers[i]);
        }
    }
    const auto peak = engine.masterPeak.load(std::memory_order_relaxed);
    const auto rms = engine.masterRms.load(std::memory_order_relaxed);
    auto meter = "PK " + dbfsText(peak) + "  /  RMS " + dbfsText(rms);
    if (!audioReady.load()) meter = text("No audio device / unsupported rate", "Brak urządzenia audio / nieobsługiwana częstotliwość");
    if (engine.clipped.load(std::memory_order_relaxed)) meter += text("  /  CLIPPING — LOWER GAIN", "  /  PRZESTER — ZMNIEJSZ GŁOŚNOŚĆ");
    meterLabel.setText("MASTER: " + meter, juce::dontSendNotification);
}
void MainComponent::statusMessage(const juce::String& message) {
    status.setText(message, juce::dontSendNotification); status.setTooltip(message);
    juce::Logger::writeToLog(message);
}
void MainComponent::browse(std::size_t deck) {
    if (chooser || loading[deck]) return;
    chooser = std::make_unique<juce::FileChooser>(text("Load a track", "Wczytaj utwór"), juce::File{}, "*.wav;*.aiff;*.aif;*.flac;*.ogg;*.mp3");
    juce::Component::SafePointer<MainComponent> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe, deck](const juce::FileChooser& fc) {
            const auto file = fc.getResult();
            if (safe) { safe->chooser.reset(); if (file.existsAsFile()) safe->load(deck, file); }
        });
}
void MainComponent::load(std::size_t deck, const juce::File& file) {
    if (loading[deck] || !file.existsAsFile()) return;
    loading[deck] = true; decks[deck]->setLoading(true);
    juce::Component::SafePointer<MainComponent> safe(this);
    const auto stop = cancelled;
    loaders.addJob([safe, stop, file, deck] {
        auto result = std::make_shared<DecodeResult>(decodeTrack(file, *stop));
        if (stop->load()) return;
        juce::MessageManager::callAsync([safe, result, deck, file] {
            if (!safe) return;
            safe->loading[deck] = false; safe->decks[deck]->setLoading(false);
            auto* submittedClip = result->clip.get();
            const double loadedDuration = submittedClip && submittedClip->valid()
                ? static_cast<double>(submittedClip->frames()) / submittedClip->sampleRate : 0.0;
            if (result->clip && safe->engine.submit(deck, std::move(result->clip))) {
                safe->closeTempoSegmentEditorForDeck(deck);
                if (safe->performanceDecks[deck]) safe->performanceDecks[deck]->resetForClip();
                safe->syncFollowers[deck] = false;
                if (safe->syncMasterDeck && *safe->syncMasterDeck == deck) {
                    safe->syncMasterDeck.reset();
                    safe->clearSyncFollowers();
                }
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
                if (safe->keyLockResearchEnabled)
                    safe->keyLockLifecycle.noteClipSubmitted(deck, submittedClip);
#endif
                safe->gridEditGeneration[deck].fetch_add(1, std::memory_order_acq_rel);
                const auto hotCueGeneration = safe->hotCueGeneration[deck].fetch_add(1, std::memory_order_acq_rel) + 1;
                safe->deckFiles[deck] = file;
                safe->detectedBeatGrids[deck] = {};
                safe->beatGrids[deck] = {};
                safe->gridIsManual[deck] = false;
                safe->decks[deck]->setTrack(result->name, std::move(result->peaks));
                safe->restoreHotCues(deck, file, loadedDuration, hotCueGeneration);
                safe->startTrackAnalysis(deck, file);
                safe->statusMessage(text("Loaded: ", "Wczytano: ") + result->name);
            } else {
                safe->decks[deck]->setTrack(text("Import failed — previous audio kept", "Błąd importu — poprzednie audio zachowane"), {});
                safe->statusMessage(result->error.isNotEmpty() ? result->error : text("Import failed.", "Błąd importu."));
            }
        });
    });
}
void MainComponent::startTrackAnalysis(std::size_t deck, const juce::File& file) {
    if (deck >= broke::deckCount || !file.existsAsFile()) return;
    if (analysisCancelled[deck]) analysisCancelled[deck]->store(true);
    auto stop = std::make_shared<std::atomic<bool>>(false);
    analysisCancelled[deck] = stop;
    detectedBeatGrids[deck] = {};
    beatGrids[deck] = {};
    gridIsManual[deck] = false;
    if (performanceDecks[deck]) performanceDecks[deck]->clearReviewedGrid();
    decks[deck]->setRhythmPending();

    juce::Component::SafePointer<MainComponent> safe(this);
    analyzers.addJob([safe, stop, file, deck] {
        TrackAnalysisCache cache;
        TrackBeatGridOverrideStore overrides;
        auto result = std::make_shared<TrackRhythmAnalysis>(
            analyzeTrackRhythmCached(file, *stop, cache));
        if (stop->load()) return;

        broke::BeatGrid detected(result->beat);
        if (result->beat.valid && !detected.valid()) {
            result->beat = {};
            result->beatError = "Detected rhythm metadata failed beat-grid validation.";
            detected = {};
        }
        const auto selection = selectTrackBeatGrid(file, result->beat, overrides);
        juce::MessageManager::callAsync([safe, stop, result, detected, selection, deck, file] {
            if (!safe || stop->load() || safe->analysisCancelled[deck] != stop) return;
            if (safe->deckFiles[deck].getFullPathName() != file.getFullPathName()) return;
            safe->analysisCancelled[deck].reset();
            safe->detectedBeatGrids[deck] = detected.valid() ? detected : broke::BeatGrid{};
            safe->beatGrids[deck] = selection.grid.valid() ? selection.grid : broke::BeatGrid{};
            safe->gridIsManual[deck] = selection.manualOverride;
            if (safe->performanceDecks[deck]) {
                if (safe->beatGrids[deck].valid())
                    static_cast<void>(safe->performanceDecks[deck]->setReviewedGrid(safe->beatGrids[deck]));
                else
                    safe->performanceDecks[deck]->clearReviewedGrid();
            }
            safe->decks[deck]->setRhythmAnalysis(*result, safe->beatGrids[deck], selection.manualOverride);
        });
    });
}
void MainComponent::applyBeatGridEdit(std::size_t deck, double beatZeroSeconds, double bpm) {
    if (deck >= broke::deckCount || !beatGrids[deck].valid() || !deckFiles[deck].existsAsFile()) {
        statusMessage(text("Beat grid is not ready for editing.", "Siatka rytmu nie jest gotowa do edycji."));
        return;
    }

    auto edited = beatGrids[deck];
    if (!edited.setBeatZero(beatZeroSeconds) || !edited.setSegmentBpm(0, bpm)) {
        statusMessage(text("Rejected invalid beat-grid correction.", "Odrzucono nieprawidłową korektę siatki."));
        decks[deck]->setBeatGrid(beatGrids[deck], gridIsManual[deck]);
        return;
    }
    if (!performanceDecks[deck]
        || performanceDecks[deck]->setReviewedGrid(edited) != broke::PerformanceDeckOwner::Result::applied) {
        statusMessage(text("Rejected beat-grid correction at performance boundary.", "Odrzucono korektę siatki na granicy sterowania deckiem."));
        decks[deck]->setBeatGrid(beatGrids[deck], gridIsManual[deck]);
        return;
    }

    adoptAndPersistReviewedGrid(deck, performanceDecks[deck]->reviewedGrid(),
        text("Manual beat grid saved locally.", "Ręczna siatka rytmu zapisana lokalnie."));
}
void MainComponent::adoptAndPersistReviewedGrid(std::size_t deck, const broke::BeatGrid& edited,
                                                const juce::String& successMessage) {
    if (deck >= broke::deckCount || !edited.valid() || !deckFiles[deck].existsAsFile()) {
        statusMessage(text("Tempo-map edit could not be persisted for this deck.",
                           "Nie można zapisać edycji mapy tempa dla tego decku."));
        return;
    }

    const auto file = deckFiles[deck];
    const auto generation = gridEditGeneration[deck].fetch_add(1, std::memory_order_acq_rel) + 1;
    beatGrids[deck] = edited;
    gridIsManual[deck] = true;
    decks[deck]->setBeatGrid(edited, true);

    juce::Component::SafePointer<MainComponent> safe(this);
    analyzers.addJob([safe, file, deck, generation, edited, successMessage] {
        if (!safe || safe->gridEditGeneration[deck].load(std::memory_order_acquire) != generation) return;
        TrackBeatGridOverrideStore store;
        const bool stored = store.store(file, edited);
        juce::MessageManager::callAsync([safe, file, deck, generation, stored, successMessage] {
            if (!safe || safe->gridEditGeneration[deck].load(std::memory_order_acquire) != generation) return;
            if (safe->deckFiles[deck].getFullPathName() != file.getFullPathName()) return;
            safe->statusMessage(stored
                ? successMessage
                : text("Manual tempo map is active for this session, but local persistence failed.",
                       "Ręczna mapa tempa działa w tej sesji, ale zapis lokalny nie powiódł się."));
        });
    });
}
void MainComponent::acceptTempoSegmentEdit(std::size_t deck) {
    if (deck >= broke::deckCount || !performanceDecks[deck]
        || !performanceDecks[deck]->hasReviewedGrid()) {
        statusMessage(text("Reviewed tempo map is no longer available.",
                           "Zweryfikowana mapa tempa nie jest już dostępna."));
        return;
    }
    adoptAndPersistReviewedGrid(deck, performanceDecks[deck]->reviewedGrid(),
        text("Tempo map saved locally.", "Mapa tempa zapisana lokalnie."));
}
void MainComponent::closeTempoSegmentEditorForDeck(std::size_t deck) {
    if (!tempoSegmentDialog || !tempoSegmentDialogDeck || *tempoSegmentDialogDeck != deck) return;
    delete tempoSegmentDialog.getComponent();
    tempoSegmentDialog = nullptr;
    tempoSegmentDialogDeck.reset();
}
void MainComponent::showTempoSegmentEditor(std::size_t deck) {
    if (deck >= broke::deckCount || !tempoSegmentEditors[deck]
        || !tempoSegmentEditors[deck]->available() || !deckFiles[deck].existsAsFile()) {
        statusMessage(text("Tempo map needs a loaded track with a reviewed beat grid.",
                           "Mapa tempa wymaga wczytanego utworu ze zweryfikowaną siatką rytmu."));
        return;
    }

    if (tempoSegmentDialog) {
        if (tempoSegmentDialogDeck && *tempoSegmentDialogDeck == deck) {
            tempoSegmentDialog->toFront(true);
            return;
        }
        delete tempoSegmentDialog.getComponent();
        tempoSegmentDialog = nullptr;
        tempoSegmentDialogDeck.reset();
    }

    auto* editor = new TempoSegmentEditorComponent(
        *tempoSegmentEditors[deck],
        [this, deck] {
            const double position = engine.meter(deck).position.load(std::memory_order_acquire);
            const double duration = engine.meter(deck).duration.load(std::memory_order_acquire);
            return std::isfinite(position) && std::isfinite(duration)
                    && position >= 0.0 && duration > 0.0 && position < duration
                ? position : std::numeric_limits<double>::quiet_NaN();
        },
        [this, deck] { acceptTempoSegmentEdit(deck); },
        [this](const juce::String& message) { statusMessage(message); });

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = text("BrokeDJ / Tempo map / Deck ", "BrokeDJ / Mapa tempa / Deck ") + deckLetter(deck);
    options.dialogBackgroundColour = background;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.escapeKeyTriggersCloseButton = true;
    options.content.setOwned(editor);
    options.content->setSize(680, 230);
    options.componentToCentreAround = this;
    tempoSegmentDialog = options.launchAsync();
    tempoSegmentDialogDeck = deck;
}
void MainComponent::resetBeatGridEdit(std::size_t deck) {
    if (deck >= broke::deckCount || !deckFiles[deck].existsAsFile()) return;
    closeTempoSegmentEditorForDeck(deck);
    const auto file = deckFiles[deck];
    const auto generation = gridEditGeneration[deck].fetch_add(1, std::memory_order_acq_rel) + 1;
    beatGrids[deck] = detectedBeatGrids[deck];
    gridIsManual[deck] = false;
    if (performanceDecks[deck]) {
        if (beatGrids[deck].valid())
            static_cast<void>(performanceDecks[deck]->setReviewedGrid(beatGrids[deck]));
        else
            performanceDecks[deck]->clearReviewedGrid();
    }
    decks[deck]->setBeatGrid(beatGrids[deck], false);

    juce::Component::SafePointer<MainComponent> safe(this);
    analyzers.addJob([safe, file, deck, generation] {
        if (!safe || safe->gridEditGeneration[deck].load(std::memory_order_acquire) != generation) return;
        TrackBeatGridOverrideStore store;
        const bool erased = store.erase(file);
        juce::MessageManager::callAsync([safe, file, deck, generation, erased] {
            if (!safe || safe->gridEditGeneration[deck].load(std::memory_order_acquire) != generation) return;
            if (safe->deckFiles[deck].getFullPathName() != file.getFullPathName()) return;
            safe->statusMessage(erased
                ? text("Manual beat grid reset to detector result.", "Ręczna siatka zresetowana do wyniku detektora.")
                : text("Grid reset is active for this session, but the stored override could not be removed.",
                       "Reset siatki działa w tej sesji, ale nie udało się usunąć zapisanej korekty."));
        });
    });
}
void MainComponent::setWholeTrackLoop(std::size_t deck, bool enabled) {
    if (deck >= broke::deckCount || !performanceDecks[deck]) return;
    static_cast<void>(performanceDecks[deck]->setWholeTrackLoop(enabled));
}
bool MainComponent::setBeatLoop(std::size_t deck, double beats, bool enabled) {
    if (deck >= broke::deckCount || !performanceDecks[deck]) return false;
    auto& owner = *performanceDecks[deck];
    if (!enabled) {
        owner.disarmLoop();
        statusMessage(text("Beat loop disabled.", "Pętla beatowa wyłączona."));
        return true;
    }

    const auto result = owner.armBeatLoopFromTransport(beats, broke::QuantizeDirection::previous);
    if (result == broke::PerformanceDeckOwner::Result::applied) {
        const bool wasMaster = syncMasterDeck && *syncMasterDeck == deck;
        if (wasMaster) clearSyncFollowers();
        else syncFollowers[deck] = false;
        statusMessage(text("Beat loop armed: ", "Pętla beatowa: ") + juce::String(beats, 0)
                      + (wasMaster
                          ? text(" beats. Master transport ownership released all Sync followers.",
                                 " beatów. Transport mastera zwolnił wszystkie followery Sync.")
                          : text(" beats. Continuous Sync released for this deck.",
                                 " beatów. Ciągły Sync zwolniony dla tego decku.")));
        return true;
    }
    if (result == broke::PerformanceDeckOwner::Result::gridUnavailable)
        statusMessage(text("Beat loop needs a valid reviewed beat grid.", "Pętla beatowa wymaga poprawnej zweryfikowanej siatki rytmu."));
    else if (result == broke::PerformanceDeckOwner::Result::trackUnavailable)
        statusMessage(text("Beat loop transport is not ready yet.", "Transport nie jest jeszcze gotowy do pętli beatowej."));
    else if (result == broke::PerformanceDeckOwner::Result::outsideTrack)
        statusMessage(text("Beat loop would extend beyond this track.", "Pętla beatowa wyszłaby poza koniec utworu."));
    else if (result == broke::PerformanceDeckOwner::Result::rendererBusy)
        statusMessage(text("Beat loop is unavailable while another deck renderer owns this path.", "Pętla beatowa jest niedostępna, gdy ten deck używa innego renderera."));
    else
        statusMessage(text("Beat loop request was rejected safely.", "Żądanie pętli beatowej zostało bezpiecznie odrzucone."));
    return false;
}
bool MainComponent::setReverseSlipMode(std::size_t deck, broke::PerformanceDeckOwner::ReverseSlipMode mode) {
    if (deck >= broke::deckCount || !performanceDecks[deck]) return false;
    const auto duration = engine.meter(deck).duration.load(std::memory_order_acquire);
    if (!std::isfinite(duration) || duration <= 0.0) {
        statusMessage(text("Reverse / Slip needs a loaded playable track.",
                           "Reverse / Slip wymaga wczytanego odtwarzalnego utworu."));
        return false;
    }
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
    if (keyLockResearchEnabled && mode != broke::PerformanceDeckOwner::ReverseSlipMode::forward)
        keyLockLifecycle.noteTransportControlChanged(deck);
#endif
    const auto result = performanceDecks[deck]->setReverseSlipMode(mode);
    if (result == broke::PerformanceDeckOwner::Result::applied) {
        if (mode != broke::PerformanceDeckOwner::ReverseSlipMode::forward) {
            if (syncMasterDeck && *syncMasterDeck == deck) clearSyncFollowers();
            else syncFollowers[deck] = false;
        }
        switch (mode) {
            case broke::PerformanceDeckOwner::ReverseSlipMode::forward:
                statusMessage(text("Reverse / Slip disabled.", "Reverse / Slip wyłączone."));
                break;
            case broke::PerformanceDeckOwner::ReverseSlipMode::reverse:
                statusMessage(text("Reverse enabled.", "Reverse włączony."));
                break;
            case broke::PerformanceDeckOwner::ReverseSlipMode::slipArmed:
                statusMessage(text("Slip armed; Reverse can now use a split transport.",
                                   "Slip uzbrojony; Reverse może teraz użyć rozdzielonego transportu."));
                break;
            case broke::PerformanceDeckOwner::ReverseSlipMode::slipReverse:
                statusMessage(text("Slip Reverse enabled; hidden transport continues forward.",
                                   "Slip Reverse włączony; ukryty transport idzie dalej do przodu."));
                break;
        }
        return true;
    }
    if (result == broke::PerformanceDeckOwner::Result::rendererBusy)
        statusMessage(text("Reverse / Slip is unavailable while Beat Loop owns this deck transport.",
                           "Reverse / Slip jest niedostępne, gdy Pętla Beat steruje transportem decku."));
    else
        statusMessage(text("Reverse / Slip request was rejected safely.",
                           "Żądanie Reverse / Slip zostało bezpiecznie odrzucone."));
    return false;
}
void MainComponent::handleHotCue(std::size_t deck, std::size_t slot, bool clear) {
    if (deck >= broke::deckCount || slot >= broke::PerformanceDeckOwner::hotCueCount
        || !performanceDecks[deck] || !deckFiles[deck].existsAsFile()) return;

    auto& owner = *performanceDecks[deck];
    const auto file = deckFiles[deck];
    if (clear) {
        owner.clearHotCue(slot);
        const auto generation = hotCueGeneration[deck].fetch_add(1, std::memory_order_acq_rel) + 1;
        persistHotCues(deck, file, generation, owner.hotCueBank());
        statusMessage(text("Hot cue cleared: ", "Usunięto hot cue: ") + juce::String(static_cast<int>(slot + 1)) + ".");
        return;
    }

    const auto& cueState = owner.hotCue(slot);
    if (cueState.set) {
        const auto duration = engine.meter(deck).duration.load(std::memory_order_acquire);
        const auto result = owner.triggerHotCue(slot, duration);
        if (result == broke::PerformanceDeckOwner::Result::applied) {
            if (syncMasterDeck && *syncMasterDeck == deck) clearSyncFollowers();
            else syncFollowers[deck] = false;
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
            if (keyLockResearchEnabled && std::isfinite(duration) && duration > 0.0)
                keyLockLifecycle.noteSeekNormalized(deck, cueState.seconds / duration);
#endif
            statusMessage(text("Hot cue triggered: ", "Uruchomiono hot cue: ") + juce::String(static_cast<int>(slot + 1)) + ".");
        } else {
            statusMessage(text("Hot cue jump was rejected safely.", "Skok hot cue został bezpiecznie odrzucony."));
        }
        return;
    }

    const bool quantize = owner.hasReviewedGrid();
    const auto result = owner.storeHotCueFromTransport(slot, quantize, 1.0, broke::QuantizeDirection::nearest);
    if (result == broke::PerformanceDeckOwner::Result::applied) {
        const auto generation = hotCueGeneration[deck].fetch_add(1, std::memory_order_acq_rel) + 1;
        persistHotCues(deck, file, generation, owner.hotCueBank());
        statusMessage((quantize
            ? text("Quantized hot cue saved: ", "Zapisano kwantyzowany hot cue: ")
            : text("Hot cue saved without grid quantization: ", "Zapisano hot cue bez kwantyzacji siatki: "))
            + juce::String(static_cast<int>(slot + 1)) + ".");
    } else if (result == broke::PerformanceDeckOwner::Result::trackUnavailable) {
        statusMessage(text("Hot cue transport is not ready yet.", "Transport nie jest jeszcze gotowy dla hot cue."));
    } else if (result == broke::PerformanceDeckOwner::Result::outsideTrack) {
        statusMessage(text("Hot cue position is outside the playable track.", "Pozycja hot cue jest poza odtwarzalnym utworem."));
    } else {
        statusMessage(text("Hot cue request was rejected safely.", "Żądanie hot cue zostało bezpiecznie odrzucone."));
    }
}
bool MainComponent::jumpBeats(std::size_t deck, double beats) {
    if (deck >= broke::deckCount || !performanceDecks[deck]) return false;
    const auto result = performanceDecks[deck]->jumpBeatsFromTransport(beats);
    if (result == broke::PerformanceDeckOwner::Result::applied) {
        if (syncMasterDeck && *syncMasterDeck == deck) clearSyncFollowers();
        else syncFollowers[deck] = false;
        const auto target = engine.control(deck).seek.load(std::memory_order_acquire);
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
        if (keyLockResearchEnabled && std::isfinite(target) && target >= 0.0)
            keyLockLifecycle.noteSeekNormalized(deck, target);
#endif
        statusMessage(text("Beat jump: ", "Skok beatów: ") + juce::String(beats, 0)
                      + text(" beats.", " beatów."));
        return true;
    }
    if (result == broke::PerformanceDeckOwner::Result::gridUnavailable)
        statusMessage(text("Beat Jump needs a valid reviewed beat grid.", "Beat Jump wymaga poprawnej zweryfikowanej siatki rytmu."));
    else if (result == broke::PerformanceDeckOwner::Result::trackUnavailable)
        statusMessage(text("Beat Jump transport is not ready yet.", "Transport nie jest jeszcze gotowy dla Beat Jump."));
    else if (result == broke::PerformanceDeckOwner::Result::outsideTrack)
        statusMessage(text("Beat Jump would leave the playable track.", "Beat Jump wyszedłby poza odtwarzalny utwór."));
    else
        statusMessage(text("Beat Jump was rejected safely.", "Beat Jump został bezpiecznie odrzucony."));
    return false;
}
void MainComponent::clearSyncFollowers() noexcept {
    syncFollowers.fill(false);
}
void MainComponent::setSyncMaster(std::size_t deck, bool enabled) {
    if (deck >= broke::deckCount || !performanceDecks[deck]) return;
    const auto duration = engine.meter(deck).duration.load(std::memory_order_acquire);
    if (enabled) {
        if (!performanceDecks[deck]->hasReviewedGrid() || !std::isfinite(duration) || duration <= 0.0) {
            statusMessage(text("Sync master request rejected; the current valid master is unchanged. Load a track with a reviewed beat grid first.",
                               "Odrzucono zmianę mastera Sync; obecny poprawny master pozostaje bez zmian. Najpierw wczytaj utwór ze zweryfikowaną siatką rytmu."));
            return;
        }
        if (!syncMasterDeck || *syncMasterDeck != deck)
            clearSyncFollowers();
        syncFollowers[deck] = false;
        syncMasterDeck = deck;
        statusMessage(text("Sync master: deck ", "Master Sync: deck ") + deckLetter(deck) + ".");
        return;
    }
    if (syncMasterDeck && *syncMasterDeck == deck) {
        syncMasterDeck.reset();
        clearSyncFollowers();
        statusMessage(text("Sync master cleared; continuous follower locks released.",
                           "Master Sync wyłączony; zwolniono ciągłe blokady followerów."));
    }
}
bool MainComponent::syncDeck(std::size_t followerDeck) {
    if (followerDeck >= broke::deckCount || !performanceDecks[followerDeck]) return false;
    if (!syncMasterDeck || *syncMasterDeck >= broke::deckCount || *syncMasterDeck == followerDeck
        || !performanceDecks[*syncMasterDeck]) {
        statusMessage(text("Select a different reviewed-grid MASTER before using SYNC.", "Przed użyciem SYNC wybierz inny deck MASTER ze zweryfikowaną siatką."));
        return false;
    }

    const auto masterDeck = *syncMasterDeck;
    const auto result = performanceDecks[followerDeck]->syncToTransport(*performanceDecks[masterDeck]);
    if (result == broke::PerformanceDeckOwner::Result::applied) {
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
        if (keyLockResearchEnabled)
            keyLockLifecycle.noteTransportControlChanged(followerDeck);
#endif
        return true;
    }
    if (result == broke::PerformanceDeckOwner::Result::gridUnavailable)
        statusMessage(text("Sync needs reviewed beat grids on both decks.", "Sync wymaga zweryfikowanych siatek rytmu na obu deckach."));
    else if (result == broke::PerformanceDeckOwner::Result::trackUnavailable)
        statusMessage(text("Sync transport is not ready on both decks.", "Transport obu decków nie jest jeszcze gotowy dla Sync."));
    else if (result == broke::PerformanceDeckOwner::Result::outsideTrack)
        statusMessage(text("Sync phase correction would leave the playable track.", "Korekta fazy Sync wyszłaby poza odtwarzalny utwór."));
    else if (result == broke::PerformanceDeckOwner::Result::rendererBusy)
        statusMessage(text("Sync is unavailable while another renderer owns this deck path.", "Sync jest niedostępny, gdy ten deck jest używany przez inny renderer."));
    else
        statusMessage(text("Sync was rejected by the bounded rate/phase safety limits.", "Sync został odrzucony przez bezpieczne limity tempa/fazy."));
    return false;
}
bool MainComponent::setSyncLock(std::size_t followerDeck, bool enabled) {
    if (followerDeck >= broke::deckCount || !performanceDecks[followerDeck]) return false;
    if (!enabled) {
        const bool wasLocked = syncFollowers[followerDeck];
        syncFollowers[followerDeck] = false;
        if (wasLocked)
            statusMessage(text("Continuous Sync released on deck ", "Ciągły Sync zwolniony na decku ")
                          + deckLetter(followerDeck) + ".");
        return true;
    }
    if (syncMasterDeck && *syncMasterDeck == followerDeck) {
        syncFollowers[followerDeck] = false;
        statusMessage(text("The MASTER deck cannot also be a Sync follower.",
                           "Deck MASTER nie może jednocześnie być followerem Sync."));
        return false;
    }
    if (!syncDeck(followerDeck)) {
        syncFollowers[followerDeck] = false;
        return false;
    }
    syncFollowers[followerDeck] = true;
    statusMessage(text("Continuous Sync locked: deck ", "Ciągły Sync zablokowany: deck ")
                  + deckLetter(followerDeck) + text(" follows ", " podąża za ")
                  + deckLetter(*syncMasterDeck) + ".");
    return true;
}
void MainComponent::serviceContinuousSync() {
    if (!syncMasterDeck || *syncMasterDeck >= broke::deckCount
        || !performanceDecks[*syncMasterDeck]) {
        clearSyncFollowers();
        return;
    }
    const auto masterDeck = *syncMasterDeck;
    const auto masterDuration = engine.meter(masterDeck).duration.load(std::memory_order_acquire);
    if (!performanceDecks[masterDeck]->hasReviewedGrid()
        || !std::isfinite(masterDuration) || masterDuration <= 0.0) {
        syncMasterDeck.reset();
        clearSyncFollowers();
        return;
    }
    if (!engine.control(masterDeck).playing.load(std::memory_order_acquire)
        || decks[masterDeck]->jogScratchActive()) {
        return;
    }

    for (std::size_t follower = 0; follower < broke::deckCount; ++follower) {
        if (!syncFollowers[follower] || follower == masterDeck || !performanceDecks[follower]) continue;
        if (!engine.control(follower).playing.load(std::memory_order_acquire)
            || decks[follower]->jogScratchActive()) {
            continue;
        }
        const auto outcome = performanceDecks[follower]->maintainSyncToTransport(
            *performanceDecks[masterDeck]);
        if (outcome.result == broke::PerformanceDeckOwner::Result::applied) {
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
            if (keyLockResearchEnabled && (outcome.rateChanged || outcome.phaseCorrected))
                keyLockLifecycle.noteTransportControlChanged(follower);
#endif
            continue;
        }

        syncFollowers[follower] = false;
        juce::String reason = text("transport conflict", "konflikt transportu");
        if (outcome.result == broke::PerformanceDeckOwner::Result::gridUnavailable)
            reason = text("reviewed grid unavailable", "brak zweryfikowanej siatki");
        else if (outcome.result == broke::PerformanceDeckOwner::Result::trackUnavailable)
            reason = text("track unavailable", "utwór niedostępny");
        else if (outcome.result == broke::PerformanceDeckOwner::Result::outsideTrack)
            reason = text("track boundary", "granica utworu");
        else if (outcome.result == broke::PerformanceDeckOwner::Result::invalidRequest)
            reason = text("phase/rate outside safety envelope", "faza/tempo poza bezpiecznym zakresem");
        statusMessage(text("Continuous Sync released on deck ", "Ciągły Sync zwolniony na decku ")
                      + deckLetter(follower) + ": " + reason + ".");
    }
}
void MainComponent::restoreHotCues(std::size_t deck, const juce::File& file,
                                   double trackDurationSeconds, std::uint64_t generation) {
    if (deck >= broke::deckCount || !file.existsAsFile()
        || !std::isfinite(trackDurationSeconds) || trackDurationSeconds <= 0.0) return;
    juce::Component::SafePointer<MainComponent> safe(this);
    analyzers.addJob([safe, file, deck, trackDurationSeconds, generation] {
        if (!safe || safe->hotCueGeneration[deck].load(std::memory_order_acquire) != generation) return;
        TrackHotCueStore store;
        TrackHotCueSnapshot snapshot;
        if (!store.load(file, snapshot)) return;
        juce::MessageManager::callAsync([safe, file, deck, trackDurationSeconds, generation, snapshot] {
            if (!safe || safe->hotCueGeneration[deck].load(std::memory_order_acquire) != generation) return;
            if (safe->deckFiles[deck].getFullPathName() != file.getFullPathName()
                || !safe->performanceDecks[deck]) return;
            const auto result = safe->performanceDecks[deck]->restoreHotCueBank(snapshot.cues, trackDurationSeconds);
            if (result == broke::PerformanceDeckOwner::Result::applied) {
                const auto restored = setHotCueCount(snapshot.cues);
                if (restored > 0)
                    safe->statusMessage(text("Restored hot cues: ", "Przywrócono hot cue: ") + juce::String(static_cast<int>(restored)) + ".");
            } else {
                safe->statusMessage(text("Stored hot cues were rejected for this changed track.", "Zapisane hot cue odrzucono dla zmienionego utworu."));
            }
        });
    });
}
void MainComponent::persistHotCues(std::size_t deck, const juce::File& file,
                                   std::uint64_t generation,
                                   broke::PerformanceDeckOwner::HotCueBank snapshot) {
    if (deck >= broke::deckCount || !file.existsAsFile()) return;
    juce::Component::SafePointer<MainComponent> safe(this);
    analyzers.addJob([safe, file, deck, generation, snapshot] {
        if (!safe || safe->hotCueGeneration[deck].load(std::memory_order_acquire) != generation) return;
        TrackHotCueStore store;
        TrackHotCueSnapshot state;
        state.cues = snapshot;
        const bool stored = store.store(file, state);
        juce::MessageManager::callAsync([safe, file, deck, generation, stored] {
            if (!safe || safe->hotCueGeneration[deck].load(std::memory_order_acquire) != generation) return;
            if (safe->deckFiles[deck].getFullPathName() != file.getFullPathName()) return;
            if (!stored)
                safe->statusMessage(text("Hot cues remain active for this session, but local persistence failed.",
                                         "Hot cue pozostają aktywne w tej sesji, ale zapis lokalny nie powiódł się."));
        });
    });
}
void MainComponent::showAudioSettings() {
    if (audioSettings) { audioSettings->toFront(true); return; }
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = text("BrokeDJ / Audio outputs", "BrokeDJ / Wyjścia audio");
    options.dialogBackgroundColour = background;
    options.useNativeTitleBar = true; options.resizable = true;
    options.content.setOwned(new juce::AudioDeviceSelectorComponent(deviceManager, 0, 0, 2, 4, false, false, true, false));
    options.content->setSize(620, 460);
    options.componentToCentreAround = this;
    audioSettings = options.launchAsync();
}
