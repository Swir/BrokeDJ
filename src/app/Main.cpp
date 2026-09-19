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

juce::Array<juce::var> jsonNumbers(const juce::Array<double>& values) {
    juce::Array<juce::var> result;
    result.ensureStorageAllocated(values.size());
    for (const auto value : values) result.add(value);
    return result;
}

juce::Array<juce::var> jsonNumbers(const juce::Array<int>& values) {
    juce::Array<juce::var> result;
    result.ensureStorageAllocated(values.size());
    for (const auto value : values) result.add(value);
    return result;
}

juce::Array<juce::var> jsonStrings(const juce::StringArray& values) {
    juce::Array<juce::var> result;
    result.ensureStorageAllocated(values.size());
    for (const auto& value : values) result.add(oneLine(value));
    return result;
}

int runDeviceProbe(bool ciSmoke) {
    constexpr int probeSchemaVersion = 2;
    juce::AudioDeviceManager manager;
    juce::OwnedArray<juce::AudioIODeviceType> types;
    manager.createAudioDeviceTypes(types);

    auto* root = new juce::DynamicObject();
    juce::var jsonRoot(root);
    juce::Array<juce::var> backendEntries;

    root->setProperty("schema_version", probeSchemaVersion);
    root->setProperty("mode", ciSmoke ? "ci-enumeration" : "silent-capability");
    root->setProperty("plays_audio", false);
    root->setProperty("calls_device_open", false);
    root->setProperty("starts_audio_callback", false);
    root->setProperty("creates_device_descriptors", !ciSmoke);
    root->setProperty("backend_count", types.size());

    juce::String report;
    report << "BrokeDJ audio-device capability probe\n"
           << "schema_version=" << probeSchemaVersion << "\n"
           << "mode=" << (ciSmoke ? "ci-enumeration" : "silent-capability") << "\n"
           << "plays_audio=no\n"
           << "opens_device=no\n"
           << "calls_device_open=no\n"
           << "starts_audio_callback=no\n"
           << "creates_device_descriptors=" << (ciSmoke ? "no" : "yes") << "\n"
           << "backend_count=" << types.size() << "\n";

    int totalOutputDevices = 0;
    int fourOutputCandidates = 0;
    int unexpectedOpenStates = 0;

    for (int typeIndex = 0; typeIndex < types.size(); ++typeIndex) {
        auto* type = types[typeIndex];
        if (type == nullptr) continue;

        type->scanForDevices();
        const auto outputs = type->getDeviceNames(false);
        const int defaultIndex = type->getDefaultDeviceIndex(false);
        totalOutputDevices += outputs.size();

        auto* backend = new juce::DynamicObject();
        juce::Array<juce::var> deviceEntries;
        backend->setProperty("index", typeIndex);
        backend->setProperty("name", oneLine(type->getTypeName()));
        backend->setProperty("output_count", outputs.size());
        backend->setProperty("default_output_index", defaultIndex);
        backend->setProperty("descriptor_details_skipped", ciSmoke);

        report << "backend[" << typeIndex << "].name=" << oneLine(type->getTypeName()) << "\n"
               << "backend[" << typeIndex << "].output_count=" << outputs.size() << "\n"
               << "backend[" << typeIndex << "].default_output_index=" << defaultIndex << "\n";

        if (!ciSmoke) {
            for (int deviceIndex = 0; deviceIndex < outputs.size(); ++deviceIndex) {
                const auto outputName = outputs[deviceIndex];
                auto* deviceEntry = new juce::DynamicObject();
                deviceEntry->setProperty("index", deviceIndex);
                deviceEntry->setProperty("name", oneLine(outputName));
                deviceEntry->setProperty("is_default", deviceIndex == defaultIndex);

                report << "device[" << typeIndex << ":" << deviceIndex << "].name="
                       << oneLine(outputName) << "\n";

                std::unique_ptr<juce::AudioIODevice> device(type->createDevice(outputName, {}));
                if (!device) {
                    deviceEntry->setProperty("descriptor_created", false);
                    report << "device[" << typeIndex << ":" << deviceIndex << "].created=no\n";
                    deviceEntries.add(juce::var(deviceEntry));
                    continue;
                }

                deviceEntry->setProperty("descriptor_created", true);
                report << "device[" << typeIndex << ":" << deviceIndex << "].created=yes\n";
                const bool openBeforeQuery = device->isOpen();
                deviceEntry->setProperty("open_before_capability_query", openBeforeQuery);
                if (openBeforeQuery) {
                    ++unexpectedOpenStates;
                    report << "device[" << typeIndex << ":" << deviceIndex
                           << "].unexpected_open_before_query=yes\n";
                    deviceEntries.add(juce::var(deviceEntry));
                    continue;
                }

                const auto outputChannels = device->getOutputChannelNames();
                const auto sampleRates = device->getAvailableSampleRates();
                const auto bufferSizes = device->getAvailableBufferSizes();
                const bool fourOutputCandidate = outputChannels.size() >= 4;
                if (fourOutputCandidate) ++fourOutputCandidates;

                const bool openAfterQuery = device->isOpen();
                deviceEntry->setProperty("open_after_capability_query", openAfterQuery);
                if (openAfterQuery) ++unexpectedOpenStates;

                deviceEntry->setProperty("output_channels", outputChannels.size());
                deviceEntry->setProperty("four_output_candidate", fourOutputCandidate);
                deviceEntry->setProperty("channel_names", jsonStrings(outputChannels));
                deviceEntry->setProperty("sample_rates_hz", jsonNumbers(sampleRates));
                deviceEntry->setProperty("buffer_sizes_samples", jsonNumbers(bufferSizes));
                deviceEntry->setProperty("default_buffer_samples", device->getDefaultBufferSize());

                report << "device[" << typeIndex << ":" << deviceIndex << "].open_before_query=no\n"
                       << "device[" << typeIndex << ":" << deviceIndex << "].output_channels="
                       << outputChannels.size() << "\n"
                       << "device[" << typeIndex << ":" << deviceIndex << "].four_output_candidate="
                       << (fourOutputCandidate ? "yes" : "no") << "\n"
                       << "device[" << typeIndex << ":" << deviceIndex << "].channel_names="
                       << outputChannels.joinIntoString(",") << "\n"
                       << "device[" << typeIndex << ":" << deviceIndex << "].sample_rates_hz="
                       << joinedNumbers(sampleRates) << "\n"
                       << "device[" << typeIndex << ":" << deviceIndex << "].buffer_sizes_samples="
                       << joinedNumbers(bufferSizes) << "\n"
                       << "device[" << typeIndex << ":" << deviceIndex << "].default_buffer_samples="
                       << device->getDefaultBufferSize() << "\n"
                       << "device[" << typeIndex << ":" << deviceIndex << "].open_after_query="
                       << (openAfterQuery ? "yes" : "no") << "\n";

                deviceEntries.add(juce::var(deviceEntry));
            }
        }

        backend->setProperty("devices", deviceEntries);
        backendEntries.add(juce::var(backend));
    }

    root->setProperty("output_device_count", totalOutputDevices);
    root->setProperty("four_output_candidate_count",
                      ciSmoke ? juce::var() : juce::var(fourOutputCandidates));
    root->setProperty("unexpected_open_state_count", unexpectedOpenStates);
    root->setProperty("safety_invariants_ok", unexpectedOpenStates == 0);
    root->setProperty("qualification_note",
                      "Capability discovery only; this does not prove device switching, physical outputs 3/4 cue isolation, latency, xrun behavior, or listening quality.");
    root->setProperty("backends", backendEntries);

    report << "output_device_count=" << totalOutputDevices << "\n";
    if (!ciSmoke) report << "four_output_candidate_count=" << fourOutputCandidates << "\n";
    report << "unexpected_open_state_count=" << unexpectedOpenStates << "\n"
           << "safety_invariants_ok=" << (unexpectedOpenStates == 0 ? "yes" : "no") << "\n"
           << "qualification_note=Capability discovery only; this does not prove device switching, "
              "physical outputs 3/4 cue isolation, latency, xrun behavior, or listening quality.\n";

    const auto directory = juce::File::getCurrentWorkingDirectory();
    const auto textOutput = directory.getChildFile("BrokeDJ-device-probe.txt");
    const auto jsonOutput = directory.getChildFile("BrokeDJ-device-probe.json");
    const auto json = juce::JSON::toString(jsonRoot, false) + "\n";

    const bool textWritten = textOutput.replaceWithText(report);
    const bool jsonWritten = jsonOutput.replaceWithText(json);
    if (!textWritten || !jsonWritten) {
        juce::Logger::writeToLog("Audio device probe could not write both report files.");
        return 2;
    }

    juce::Logger::writeToLog("Audio device probe written: " + textOutput.getFullPathName());
    juce::Logger::writeToLog("Audio device probe JSON written: " + jsonOutput.getFullPathName());
    if (unexpectedOpenStates != 0) {
        juce::Logger::writeToLog("Audio device probe safety invariant failed: an unexpectedly open descriptor was observed.");
        return 3;
    }
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
