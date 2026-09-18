// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <JuceHeader.h>
#include "core/Engine.h"
#include "Decoder.h"

juce::String text(const char* english, const char* polish);
class Waveform final : public juce::Component {
public:
    std::vector<float> peaks;
    float progress = 0;
    std::function<void(double)> onSeek;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent& event) override;
};
class DeckPanel final : public juce::Component, public juce::FileDragAndDropTarget {
public:
    DeckPanel(broke::Engine&, std::size_t);
    std::function<void()> onBrowse;
    std::function<void(const juce::File&)> onDrop;
    void setTrack(const juce::String&, std::vector<float>);
    void setLoading(bool);
    void refresh();
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;
private:
    broke::Engine& engine;
    std::size_t index;
    juce::Label heading, track, time;
    Waveform waveform;
    juce::TextButton load, play, rewind, loop, cue;
    std::array<juce::Slider, 7> knobs;
    std::array<juce::Label, 7> knobNames;
};
class MainComponent final : public juce::AudioAppComponent, private juce::Timer {
public:
    explicit MainComponent(bool openAudio = true);
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
    void showAudioSettings();
    void statusMessage(const juce::String&);
    juce::LookAndFeel_V4 theme;
    broke::Engine engine;
    std::array<std::unique_ptr<DeckPanel>, broke::deckCount> decks;
    std::array<bool, broke::deckCount> loading{};
    std::shared_ptr<std::atomic<bool>> cancelled = std::make_shared<std::atomic<bool>>(false);
    juce::ThreadPool loaders{1};
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
