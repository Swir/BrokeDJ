// SPDX-License-Identifier: AGPL-3.0-only
#include <JuceHeader.h>
#include "MainComponent.h"

namespace {

juce::String oneLine(juce::String value) {
    return value.replaceCharacters("\r\n\t", "   ");
}

juce::String joinedNumbers(const juce::Array<double>& values) {
    juce::StringArray parts;
    for (const auto value : values) parts.add(juce::String(value, 0));
    return parts.joinIntoString(",");
}

juce::String joinedNumbers(const juce::Array<int>& values) {
    juce::StringArray parts;
    for (const auto value : values) parts.add(juce::String(value));
    return parts.joinIntoString(",");
}

int runDeviceProbe(bool ciSmoke) {
    juce::AudioDeviceManager manager;
    juce::OwnedArray<juce::AudioIODeviceType> types;
    manager.createAudioDeviceTypes(types);

    juce::String report;
    report << "BrokeDJ audio-device capability probe\n"
           << "mode=" << (ciSmoke ? "ci-enumeration" : "silent-capability") << "\n"
           << "plays_audio=no\nopens_device=no\n"
           << "backend_count=" << types.size() << "\n";

    int totalOutputDevices = 0;
    int fourOutputCandidates = 0;
    for (int typeIndex = 0; typeIndex < types.size(); ++typeIndex) {
        auto* type = types[typeIndex];
        if (type == nullptr) continue;

        type->scanForDevices();
        const auto outputs = type->getDeviceNames(false);
        const int defaultIndex = type->getDefaultDeviceIndex(false);
        totalOutputDevices += outputs.size();

        report << "backend[" << typeIndex << "].name=" << oneLine(type->getTypeName()) << "\n"
               << "backend[" << typeIndex << "].output_count=" << outputs.size() << "\n"
               << "backend[" << typeIndex << "].default_output_index=" << defaultIndex << "\n";

        if (ciSmoke) continue;

        for (int deviceIndex = 0; deviceIndex < outputs.size(); ++deviceIndex) {
            const auto outputName = outputs[deviceIndex];
            report << "device[" << typeIndex << ":" << deviceIndex << "].name="
                   << oneLine(outputName) << "\n";

            std::unique_ptr<juce::AudioIODevice> device(type->createDevice(outputName, {}));
            if (!device) {
                report << "device[" << typeIndex << ":" << deviceIndex << "].created=no\n";
                continue;
            }

            const auto outputChannels = device->getOutputChannelNames();
            const bool fourOutputCandidate = outputChannels.size() >= 4;
            if (fourOutputCandidate) ++fourOutputCandidates;

            report << "device[" << typeIndex << ":" << deviceIndex << "].created=yes\n"
                   << "device[" << typeIndex << ":" << deviceIndex << "].output_channels="
                   << outputChannels.size() << "\n"
                   << "device[" << typeIndex << ":" << deviceIndex << "].four_output_candidate="
                   << (fourOutputCandidate ? "yes" : "no") << "\n"
                   << "device[" << typeIndex << ":" << deviceIndex << "].channel_names="
                   << outputChannels.joinIntoString(",") << "\n"
                   << "device[" << typeIndex << ":" << deviceIndex << "].sample_rates_hz="
                   << joinedNumbers(device->getAvailableSampleRates()) << "\n"
                   << "device[" << typeIndex << ":" << deviceIndex << "].buffer_sizes_samples="
                   << joinedNumbers(device->getAvailableBufferSizes()) << "\n"
                   << "device[" << typeIndex << ":" << deviceIndex << "].default_buffer_samples="
                   << device->getDefaultBufferSize() << "\n";
        }
    }

    report << "output_device_count=" << totalOutputDevices << "\n";
    if (!ciSmoke) report << "four_output_candidate_count=" << fourOutputCandidates << "\n";
    report << "qualification_note=Capability discovery only; this does not prove device switching, "
              "physical outputs 3/4 cue isolation, latency, xrun behavior, or listening quality.\n";

    const auto output = juce::File::getCurrentWorkingDirectory().getChildFile("BrokeDJ-device-probe.txt");
    if (!output.replaceWithText(report)) {
        juce::Logger::writeToLog("Audio device probe could not write: " + output.getFullPathName());
        return 2;
    }
    juce::Logger::writeToLog("Audio device probe written: " + output.getFullPathName());
    return 0;
}

} // namespace

class BrokeDJApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "BrokeDJ"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise(const juce::String& arguments) override {
        logger.reset(juce::FileLogger::createDefaultAppLogger("BrokeDJ", "BrokeDJ.log", "BrokeDJ 0.1.0 development log", 256 * 1024));
        juce::Logger::setCurrentLogger(logger.get());
        const bool deviceProbe = arguments.contains("--device-probe");
        const bool deviceProbeCi = arguments.contains("--device-probe-ci");
        if (deviceProbe || deviceProbeCi) {
            setApplicationReturnValue(runDeviceProbe(deviceProbeCi));
            quit();
            return;
        }
        const bool smokeTest = arguments.contains("--smoke-test");
        const bool keyLockResearch = arguments.contains("--key-lock-research");
        window = std::make_unique<Window>(!smokeTest, keyLockResearch);
        if (smokeTest) juce::Timer::callAfterDelay(1200, [this] { quit(); });
    }
    void shutdown() override { window.reset(); juce::Logger::setCurrentLogger(nullptr); logger.reset(); }
    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted(const juce::String&) override { if (window) window->toFront(true); }
private:
    class Window final : public juce::DocumentWindow {
    public:
        Window(bool openAudio, bool keyLockResearch)
            : DocumentWindow("BrokeDJ — by Swir", juce::Colour(0xff080e1a), allButtons) {
            setUsingNativeTitleBar(true); setContentOwned(new MainComponent(openAudio, keyLockResearch), true);
            setResizable(true, false); setResizeLimits(1050, 800, 3840, 2160);
            centreWithSize(getWidth(), getHeight()); setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<juce::FileLogger> logger;
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(BrokeDJApplication)