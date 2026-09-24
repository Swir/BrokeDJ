// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <utility>

namespace broke::app {

class AudioDeviceStateStore final {
public:
    static constexpr int schemaVersion = 1;
    static constexpr std::int64_t maxStateBytes = 128 * 1024;

    explicit AudioDeviceStateStore(juce::File stateFile = defaultStateFile())
        : file(std::move(stateFile)) {}

    [[nodiscard]] static juce::File defaultStateFile() {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("BrokeDJ")
            .getChildFile("audio-device.xml");
    }

    [[nodiscard]] const juce::File& stateFile() const noexcept { return file; }

    [[nodiscard]] std::unique_ptr<juce::XmlElement> load() const {
        if (!file.existsAsFile()) return {};
        const auto size = file.getSize();
        if (size <= 0 || size > maxStateBytes) return {};

        auto parsed = juce::XmlDocument::parse(file);
        if (!parsed || !parsed->hasTagName("DEVICESETUP")
            || parsed->getIntAttribute("brokedjStateVersion", -1) != schemaVersion) {
            return {};
        }

        return sanitise(*parsed, true);
    }

    [[nodiscard]] bool store(const juce::XmlElement& source) const {
        auto safe = sanitise(source, false);
        if (!safe) return false;
        safe->setAttribute("brokedjStateVersion", schemaVersion);

        if (file.getParentDirectory().createDirectory().failed()) return false;
        juce::TemporaryFile temporary(file);
        {
            juce::FileOutputStream output(temporary.getFile());
            if (!output.openedOk()) return false;
            safe->writeTo(output, {});
            output.flush();
            if (output.getStatus().failed()) return false;
        }
        return temporary.overwriteTargetFileWithTemporary();
    }

    [[nodiscard]] bool clear() const {
        return !file.existsAsFile() || file.deleteFile();
    }

    [[nodiscard]] static int requestedOutputChannels(const juce::XmlElement& state,
                                                     int fallback = 2) noexcept {
        const int boundedFallback = std::clamp(fallback, 2, 4);
        juce::BigInteger channels;
        if (!parseOutputMask(state, channels)) return boundedFallback;
        return channels.countNumberOfSetBits();
    }

    [[nodiscard]] static bool matchesCurrentSetup(const juce::XmlElement& state,
                                                  const juce::AudioDeviceManager& manager) {
        const auto expectedName = state.getStringAttribute(
            "audioDeviceName", state.getStringAttribute("audioOutputDeviceName"));
        const auto setup = manager.getAudioDeviceSetup();
        const auto* current = manager.getCurrentAudioDevice();

        if (expectedName.isEmpty()) {
            if (current != nullptr || setup.outputDeviceName.isNotEmpty()) return false;
        } else if (current == nullptr || !setup.outputDeviceName.equalsIgnoreCase(expectedName)) {
            return false;
        }

        const auto expectedType = state.getStringAttribute("deviceType");
        if (expectedType.isNotEmpty()
            && !manager.getCurrentAudioDeviceType().equalsIgnoreCase(expectedType)) {
            return false;
        }

        if (state.hasAttribute("audioDeviceRate")) {
            const double expectedRate = state.getDoubleAttribute("audioDeviceRate", 0.0);
            if (expectedRate > 0.0 && std::abs(setup.sampleRate - expectedRate) > 0.5)
                return false;
        }
        if (state.hasAttribute("audioDeviceBufferSize")) {
            const int expectedBuffer = state.getIntAttribute("audioDeviceBufferSize", 0);
            if (expectedBuffer > 0 && setup.bufferSize != expectedBuffer) return false;
        }
        if (state.hasAttribute("audioDeviceOutChans")) {
            juce::BigInteger expectedChannels;
            if (!parseOutputMask(state, expectedChannels)
                || setup.useDefaultOutputChannels
                || setup.outputChannels != expectedChannels) {
                return false;
            }
        }
        return true;
    }

private:
    [[nodiscard]] static bool parseOutputMask(const juce::XmlElement& state,
                                              juce::BigInteger& channels) noexcept {
        if (!state.hasAttribute("audioDeviceOutChans")) return false;
        const auto mask = state.getStringAttribute("audioDeviceOutChans").trim();
        if (mask.isEmpty() || mask.length() > 256 || !mask.containsOnly("01")) return false;

        channels.clear();
        channels.parseString(mask, 2);
        const int active = channels.countNumberOfSetBits();
        return active >= 2 && active <= 4;
    }

    [[nodiscard]] static std::unique_ptr<juce::XmlElement> sanitise(
        const juce::XmlElement& source, bool requireSchema) {
        if (!source.hasTagName("DEVICESETUP")) return {};
        if (requireSchema
            && source.getIntAttribute("brokedjStateVersion", -1) != schemaVersion) {
            return {};
        }

        auto result = std::make_unique<juce::XmlElement>("DEVICESETUP");
        const auto copyBoundedString = [&](const char* name) {
            if (!source.hasAttribute(name)) return;
            const auto value = source.getStringAttribute(name);
            if (value.length() <= 1024) result->setAttribute(name, value);
        };

        copyBoundedString("deviceType");
        copyBoundedString("audioOutputDeviceName");

        if (source.hasAttribute("audioDeviceRate")) {
            const double rate = source.getDoubleAttribute("audioDeviceRate", 0.0);
            if (std::isfinite(rate) && rate > 0.0) result->setAttribute("audioDeviceRate", rate);
        }
        if (source.hasAttribute("audioDeviceBufferSize")) {
            const int buffer = source.getIntAttribute("audioDeviceBufferSize", 0);
            if (buffer > 0 && buffer <= 1048576)
                result->setAttribute("audioDeviceBufferSize", buffer);
        }

        juce::BigInteger outputChannels;
        if (parseOutputMask(source, outputChannels))
            result->setAttribute("audioDeviceOutChans", outputChannels.toString(2));

        result->setAttribute("brokedjStateVersion", schemaVersion);
        return result;
    }

    juce::File file;
};

// Keeps JUCE's existing AudioAppComponent callback topology while adding only a
// message-thread persistence boundary around explicit output-device changes.
// No state I/O occurs in prepareToPlay(), getNextAudioBlock(), or Engine::process().
class PersistentAudioAppComponent : public juce::AudioAppComponent,
                                    private juce::ChangeListener {
public:
    PersistentAudioAppComponent() = default;

    ~PersistentAudioAppComponent() override {
        if (listening) deviceManager.removeChangeListener(this);
    }

protected:
    void setAudioChannels(int numInputChannels, int numOutputChannels) {
        if (listening) {
            deviceManager.removeChangeListener(this);
            listening = false;
        }

        auto saved = stateStore.load();
        const int requestedOutputs = saved
            ? AudioDeviceStateStore::requestedOutputChannels(*saved, numOutputChannels)
            : std::clamp(numOutputChannels, 2, 4);

        juce::AudioAppComponent::setAudioChannels(
            numInputChannels, requestedOutputs, saved.get());

        // JUCE can fall back to the default device when a saved device/rate,
        // buffer size, or explicit output-channel selection is unavailable.
        // Do not keep replaying a stale identity on every launch.
        if (saved && !AudioDeviceStateStore::matchesCurrentSetup(*saved, deviceManager))
            static_cast<void>(stateStore.clear());

        deviceManager.addChangeListener(this);
        listening = true;
    }

    // MainComponent already calls shutdownAudio() from its destructor. Shadow
    // that boundary so the last explicit device choice is persisted while the
    // device still reflects the live setup, then delegate to JUCE's shutdown.
    void shutdownAudio() {
        if (listening) {
            persistCurrentState();
            deviceManager.removeChangeListener(this);
            listening = false;
        }
        juce::AudioAppComponent::shutdownAudio();
    }

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override {
        if (source == &deviceManager) persistCurrentState();
    }

    void persistCurrentState() {
        auto state = deviceManager.createStateXml();
        if (!state) return;
        if (AudioDeviceStateStore::matchesCurrentSetup(*state, deviceManager))
            static_cast<void>(stateStore.store(*state));
        else
            static_cast<void>(stateStore.clear());
    }

    AudioDeviceStateStore stateStore;
    bool listening = false;
};

} // namespace broke::app
