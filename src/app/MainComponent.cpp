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
void configureLabel(juce::Label& label, const juce::String& value, float size = 13.0f) {
    label.setText(value, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, pale);
    label.setFont(juce::Font(juce::FontOptions(size)));
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
    configureLabel(heading, "DECK " + juce::String::charToString(static_cast<juce::juce_wchar>('A' + d)) + (d % 2 == 0 ? "  /  LEFT" : "  /  RIGHT"), 16);
    configureLabel(track, text("No track loaded", "Nie wczytano utworu"), 14);
    configureLabel(time, "00:00 / 00:00", 12);
    configureLabel(rhythm, text("BPM —  /  GRID —  /  KEY —", "BPM —  /  SIATKA —  /  TONACJA —"), 11);
    rhythm.setColour(juce::Label::textColourId, muted);
    time.setJustificationType(juce::Justification::centredRight);
    load.setButtonText(text("Load", "Wczytaj"));
    play.setButtonText("PLAY"); rewind.setButtonText("CUE 0");
    loop.setButtonText("LOOP"); cue.setButtonText(text("Headphones", "Słuchawki"));
    loop.setClickingTogglesState(true); cue.setClickingTogglesState(true);
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
        engine.control(index).loop = loop.getToggleState();
        if (onKeyLockControlChanged) onKeyLockControlChanged();
    };
    cue.onClick = [this] { engine.control(index).headphone = cue.getToggleState(); };
    waveform.onSeek = [this](double position) {
        engine.control(index).seek = position;
        if (onSeekRequested) onSeekRequested(position);
    };
    for (juce::Component* child : std::array<juce::Component*, 10>{&heading, &track, &time, &rhythm, &waveform, &load, &play, &rewind, &loop, &cue}) addAndMakeVisible(child);
    const std::array<juce::String, 7> names {text("Gain", "Głośność"), text("Rate %", "Tempo %"), "LOW", "MID", "HIGH", "ECHO", "DRIVE"};
    for (std::size_t i = 0; i < knobs.size(); ++i) {
        auto& knob = knobs[i];
        knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
        knob.setRange(i == 1 ? -20.0 : 0.0, i == 0 ? 1.5 : (i == 1 ? 20.0 : (i == 5 ? 0.7 : (i == 6 ? 6.0 : 2.0))), 0.01);
        const double initial = i == 0 ? 0.7 : (i >= 2 && i <= 4 ? 1.0 : 0.0);
        knob.setValue(initial); knob.setDoubleClickReturnValue(true, initial);
        knob.onValueChange = [this, i] {
            const auto value = static_cast<float>(knobs[i].getValue());
            auto& c = engine.control(index);
            switch (i) {
                case 0: c.gain = value; break;
                case 1: c.rate = 1.0f + value / 100.0f; break;
                case 2: c.low = value; break;
                case 3: c.mid = value; break;
                case 4: c.high = value; break;
                case 5: c.echo = value; break;
                case 6: c.drive = value; break;
                default: break;
            }
            if (i == 1 && onKeyLockControlChanged) onKeyLockControlChanged();
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
    addAndMakeVisible(gridZeroLabel); addAndMakeVisible(gridBpmLabel); addAndMakeVisible(gridReset);

    const auto gridTooltip = text(
        "Manual base-grid correction. Beat zero shifts all preserved tempo-change boundaries; GRID BPM edits segment 0 only. Changes are stored locally outside the audio callback.",
        "Ręczna korekta podstawy siatki. Zero przesuwa wszystkie zachowane zmiany tempa; BPM SIATKI edytuje tylko segment 0. Zmiany są zapisywane lokalnie poza callbackiem audio.");
    gridZero.setTooltip(gridTooltip);
    gridBpm.setTooltip(gridTooltip);
    gridReset.setTooltip(text("Erase the local manual override and return to the detected grid for this file.",
                              "Usuń lokalną ręczną korektę i wróć do wykrytej siatki dla tego pliku."));
    knobs[1].setTooltip(text("Playback rate changes pitch unless the opt-in research key-lock path is active. Key lock has no GUI control yet.", "Zmiana tempa zmienia tonację, chyba że aktywna jest testowa ścieżka key lock. Key lock nie ma jeszcze kontrolki GUI."));
    knobs[5].setTooltip(text("Fixed 250 ms echo; not beat-synchronized yet.", "Echo 250 ms; jeszcze bez synchronizacji do BPM."));
    loop.setTooltip(text("Loops the whole track, not a beat-length loop.", "Zapętla cały utwór, nie wybraną liczbę beatów."));
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
    gridZero.setEnabled(false); gridBpm.setEnabled(false); gridReset.setEnabled(false);
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
        : text("Offline tempo/grid and musical-key estimates. Current accuracy evidence is deterministic/synthetic; sync is not enabled yet.",
               "Szacunki offline tempa/siatki i tonacji. Obecna walidacja dokładności jest deterministyczna/syntetyczna; sync nie jest jeszcze włączony.");
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
    if (editable) {
        gridZero.setValue(activeBeatGrid.beatZeroSeconds(), juce::dontSendNotification);
        gridBpm.setValue(activeBeatGrid.segments().front().bpm, juce::dontSendNotification);
    }
    waveform.setBeatGrid(activeBeatGrid, manualBeatGrid);
}
void DeckPanel::refresh() {
    const auto& meter = engine.meter(index);
    const auto duration = meter.duration.load(), position = meter.position.load();
    time.setText(clockText(position) + " / " + clockText(duration), juce::dontSendNotification);
    waveform.progress = duration > 0 ? static_cast<float>(position / duration) : 0;
    waveform.setDuration(duration);
    waveform.repaint();
    play.setButtonText(engine.control(index).playing.load() ? "PAUSE" : "PLAY");
}
void DeckPanel::paint(juce::Graphics& g) {
    g.setColour(panel); g.fillRoundedRectangle(getLocalBounds().toFloat(), 12.0f);
    g.setColour(blue.withAlpha(0.25f)); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 12, 1);
}
void DeckPanel::resized() {
    auto area = getLocalBounds().reduced(14);
    auto top = area.removeFromTop(24); time.setBounds(top.removeFromRight(160)); heading.setBounds(top);
    track.setBounds(area.removeFromTop(24));
    rhythm.setBounds(area.removeFromTop(18)); area.removeFromTop(4);
    waveform.setBounds(area.removeFromTop(std::max(42, area.getHeight() - 210)));
    area.removeFromTop(6);
    auto buttons = area.removeFromTop(30);
    std::array<juce::TextButton*, 5> list {&load, &play, &rewind, &loop, &cue};
    const int buttonWidth = buttons.getWidth() / 5;
    for (auto* button : list) button->setBounds(buttons.removeFromLeft(buttonWidth).reduced(2, 0));
    area.removeFromTop(4);
    auto gridArea = area.removeFromTop(44);
    gridReset.setBounds(gridArea.removeFromRight(96).reduced(2, 7));
    auto zeroArea = gridArea.removeFromLeft(gridArea.getWidth() / 2);
    gridZeroLabel.setBounds(zeroArea.removeFromTop(14)); gridZero.setBounds(zeroArea);
    gridBpmLabel.setBounds(gridArea.removeFromTop(14)); gridBpm.setBounds(gridArea);
    area.removeFromTop(4);
    const int knobWidth = area.getWidth() / 7;
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
        decks[i] = std::make_unique<DeckPanel>(engine, i);
        decks[i]->onBrowse = [this, i] { browse(i); };
        decks[i]->onDrop = [this, i](const juce::File& file) { load(i, file); };
        decks[i]->onGridEdit = [this, i](double beatZero, double bpm) {
            applyBeatGridEdit(i, beatZero, bpm);
        };
        decks[i]->onGridReset = [this, i] { resetBeatGridEdit(i); };
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
        static_cast<void>(keyLockLifecycle.setEnabled(i, keyLockResearchEnabled));
        decks[i]->onBeforePlay = [this, i] {
            if (keyLockResearchEnabled) serviceKeyLockDeck(i, false);
        };
        decks[i]->onKeyLockControlChanged = [this, i] {
            if (keyLockResearchEnabled) keyLockLifecycle.noteTransportControlChanged(i);
        };
        decks[i]->onSeekRequested = [this, i](double position) {
            if (keyLockResearchEnabled) keyLockLifecycle.noteSeekNormalized(i, position);
        };
#endif
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
    for (auto& deck : decks) deck->refresh();
    const auto peak = engine.masterPeak.load();
    auto meter = peak > 0.000001f ? juce::String(20.0f * std::log10(peak), 1) + " dBFS" : juce::String("— dBFS");
    if (!audioReady.load()) meter = text("No audio device / unsupported rate", "Brak urządzenia audio / nieobsługiwana częstotliwość");
    if (engine.clipped.load()) meter += text("  /  CLIPPING — LOWER GAIN", "  /  PRZESTER — ZMNIEJSZ GŁOŚNOŚĆ");
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
            if (result->clip && safe->engine.submit(deck, std::move(result->clip))) {
#if defined(BROKEDJ_TIMESTRETCH_PROTOTYPE)
                if (safe->keyLockResearchEnabled)
                    safe->keyLockLifecycle.noteClipSubmitted(deck, submittedClip);
#endif
                safe->gridEditGeneration[deck].fetch_add(1, std::memory_order_acq_rel);
                safe->deckFiles[deck] = file;
                safe->detectedBeatGrids[deck] = {};
                safe->beatGrids[deck] = {};
                safe->gridIsManual[deck] = false;
                safe->decks[deck]->setTrack(result->name, std::move(result->peaks));
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

    const auto file = deckFiles[deck];
    const auto generation = gridEditGeneration[deck].fetch_add(1, std::memory_order_acq_rel) + 1;
    beatGrids[deck] = edited;
    gridIsManual[deck] = true;
    decks[deck]->setBeatGrid(edited, true);

    juce::Component::SafePointer<MainComponent> safe(this);
    analyzers.addJob([safe, file, deck, generation, edited] {
        if (!safe || safe->gridEditGeneration[deck].load(std::memory_order_acquire) != generation) return;
        TrackBeatGridOverrideStore store;
        const bool stored = store.store(file, edited);
        juce::MessageManager::callAsync([safe, file, deck, generation, stored] {
            if (!safe || safe->gridEditGeneration[deck].load(std::memory_order_acquire) != generation) return;
            if (safe->deckFiles[deck].getFullPathName() != file.getFullPathName()) return;
            safe->statusMessage(stored
                ? text("Manual beat grid saved locally.", "Ręczna siatka rytmu zapisana lokalnie.")
                : text("Manual grid is active for this session, but local persistence failed.",
                       "Ręczna siatka działa w tej sesji, ale zapis lokalny nie powiódł się."));
        });
    });
}
void MainComponent::resetBeatGridEdit(std::size_t deck) {
    if (deck >= broke::deckCount || !deckFiles[deck].existsAsFile()) return;
    const auto file = deckFiles[deck];
    const auto generation = gridEditGeneration[deck].fetch_add(1, std::memory_order_acq_rel) + 1;
    beatGrids[deck] = detectedBeatGrids[deck];
    gridIsManual[deck] = false;
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
