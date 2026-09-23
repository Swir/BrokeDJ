// SPDX-License-Identifier: AGPL-3.0-only
#include <JuceHeader.h>
#include "LibraryDatabase.h"
#include "RecordingMainComponent.h"

#include <array>
#include <filesystem>
#include <string>
#include <utility>

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

std::filesystem::path filesystemPath(const juce::File& file) {
#if JUCE_WINDOWS
    return std::filesystem::path(file.getFullPathName().toWideCharPointer());
#else
    return std::filesystem::u8path(file.getFullPathName().toStdString());
#endif
}

std::filesystem::path libraryDatabasePath() {
    return filesystemPath(
        juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("BrokeDJ")
            .getChildFile("library.sqlite3"));
}

bool libraryRecoverySmokeAllowed() {
    const auto actions = juce::SystemStats::getEnvironmentVariable("GITHUB_ACTIONS", {});
    const auto localOverride = juce::SystemStats::getEnvironmentVariable(
        "BROKEDJ_ALLOW_LOCAL_LIBRARY_RECOVERY_SMOKE", {});
    return actions.equalsIgnoreCase("true") || localOverride == "1";
}

int runLibraryRecoverySmoke(bool restore) {
    constexpr int reportSchemaVersion = 1;
    const auto directory = juce::File::getCurrentWorkingDirectory();
    const auto backupFile = directory.getChildFile("BrokeDJ-library-backup.sqlite3");
    const auto reportFile = directory.getChildFile(
        restore ? "BrokeDJ-library-restore-smoke.json" : "BrokeDJ-library-backup-smoke.json");
    const auto mode = restore ? "native-library-restore" : "native-library-backup";

    auto finish = [&](bool success, int exitCode, int databaseSchemaVersion,
                      const juce::String& errorText = {}) {
        auto* root = new juce::DynamicObject();
        juce::var report(root);
        root->setProperty("schema_version", reportSchemaVersion);
        root->setProperty("mode", mode);
        root->setProperty("plays_audio", false);
        root->setProperty("opens_audio_device", false);
        root->setProperty("starts_audio_callback", false);
        root->setProperty("destructive_fixture_only", true);
        root->setProperty("database_schema_version", databaseSchemaVersion);
        root->setProperty("backup_present", backupFile.existsAsFile());
        root->setProperty("backup_size_bytes",
                          backupFile.existsAsFile() ? backupFile.getSize() : static_cast<juce::int64>(0));
        root->setProperty("success", success);
        root->setProperty("qualification_note",
                          "CI-fixture recovery evidence only; no audio device is opened and this does not replace an interactive Windows 11 backup/restore review.");
        if (errorText.isNotEmpty()) root->setProperty("error", oneLine(errorText));

        if (!reportFile.replaceWithText(juce::JSON::toString(report, false) + "\n")) {
            juce::Logger::writeToLog("Library recovery smoke could not write its JSON report.");
            return 23;
        }
        return exitCode;
    };

    if (!libraryRecoverySmokeAllowed()) {
        return finish(false, 20, 0,
                      "Library recovery smoke is restricted to GitHub Actions or an explicit local fixture override.");
    }

    broke::library::LibraryDatabase database;
    std::string error;
    if (!database.open(libraryDatabasePath(), &error)) {
        return finish(false, 21, 0, juce::String::fromUTF8(error.c_str()));
    }

    bool operationOk = false;
    if (restore) {
        if (!backupFile.existsAsFile()) {
            return finish(false, 22, database.schemaVersion(), "Recovery smoke backup is missing.");
        }
        operationOk = database.restoreFrom(filesystemPath(backupFile), &error);
    } else {
        if (backupFile.existsAsFile() && !backupFile.deleteFile()) {
            return finish(false, 22, database.schemaVersion(), "Could not clear the previous recovery smoke backup.");
        }
        operationOk = database.backupTo(filesystemPath(backupFile), &error);
    }

    if (!operationOk || !error.empty()) {
        return finish(false, 22, database.schemaVersion(), juce::String::fromUTF8(error.c_str()));
    }
    if (!database.integrityCheck(&error) || !error.empty()) {
        return finish(false, 22, database.schemaVersion(), juce::String::fromUTF8(error.c_str()));
    }
    if (database.schemaVersion() != broke::library::LibraryDatabase::currentSchemaVersion) {
        return finish(false, 22, database.schemaVersion(), "Recovered library schema is not current.");
    }
    if (!backupFile.existsAsFile() || backupFile.getSize() <= 0) {
        return finish(false, 22, database.schemaVersion(), "Recovery smoke backup is empty or missing.");
    }

    return finish(true, 0, database.schemaVersion());
}

int runM4FixtureStateProbe() {
    constexpr int reportSchemaVersion = 1;
    const auto reportFile = juce::File::getCurrentWorkingDirectory()
                                .getChildFile("BrokeDJ-m4-fixture-state.json");

    auto finish = [&](bool success, int exitCode, int databaseSchemaVersion,
                      const broke::library::FilePresenceRefreshResult* refresh,
                      const broke::library::ContentHashWorkflowSummary* workflow,
                      const juce::String& errorText = {}) {
        auto* root = new juce::DynamicObject();
        juce::var report(root);
        root->setProperty("schema_version", reportSchemaVersion);
        root->setProperty("mode", "m4-fixture-state");
        root->setProperty("plays_audio", false);
        root->setProperty("opens_audio_device", false);
        root->setProperty("starts_audio_callback", false);
        root->setProperty("database_schema_version", databaseSchemaVersion);

        auto* refreshObject = new juce::DynamicObject();
        refreshObject->setProperty("scanned", refresh != nullptr ? static_cast<juce::int64>(refresh->scanned) : 0);
        refreshObject->setProperty("changed", refresh != nullptr ? static_cast<juce::int64>(refresh->changed) : 0);
        refreshObject->setProperty("missing", refresh != nullptr ? static_cast<juce::int64>(refresh->missing) : 0);
        refreshObject->setProperty("unresolved", refresh != nullptr ? static_cast<juce::int64>(refresh->unresolved) : 0);
        refreshObject->setProperty("complete", refresh != nullptr && refresh->complete);
        root->setProperty("refresh", juce::var(refreshObject));

        auto* workflowObject = new juce::DynamicObject();
        workflowObject->setProperty("track_count", workflow != nullptr ? static_cast<juce::int64>(workflow->trackCount) : 0);
        workflowObject->setProperty("missing_track_count", workflow != nullptr ? static_cast<juce::int64>(workflow->missingTrackCount) : 0);
        workflowObject->setProperty("history_count", workflow != nullptr ? static_cast<juce::int64>(workflow->historyCount) : 0);
        workflowObject->setProperty("tag_association_count", workflow != nullptr ? static_cast<juce::int64>(workflow->tagAssociationCount) : 0);
        workflowObject->setProperty("playlist_membership_count", workflow != nullptr ? static_cast<juce::int64>(workflow->playlistMembershipCount) : 0);
        root->setProperty("workflow", juce::var(workflowObject));

        root->setProperty("success", success);
        root->setProperty("qualification_note",
                          "Local privacy-safe aggregate verification for the disposable M4 fixture only; no track path, title, artist, tag name, playlist name or history timestamp is serialized.");
        if (errorText.isNotEmpty()) root->setProperty("error", oneLine(errorText));

        if (!reportFile.replaceWithText(juce::JSON::toString(report, false) + "\n")) {
            juce::Logger::writeToLog("M4 fixture-state probe could not write its JSON report.");
            return 39;
        }
        return exitCode;
    };

    const auto hashText = juce::SystemStats::getEnvironmentVariable("BROKEDJ_M4_FIXTURE_HASH", {}).trim();
    const auto contentHash = hashText.toStdString();
    if (contentHash.size() != 64) {
        return finish(false, 30, 0, nullptr, nullptr,
                      "BROKEDJ_M4_FIXTURE_HASH must contain one lowercase SHA-256 digest.");
    }

    broke::library::LibraryDatabase database;
    std::string error;
    if (!database.open(libraryDatabasePath(), &error)) {
        return finish(false, 31, 0, nullptr, nullptr, juce::String::fromUTF8(error.c_str()));
    }

    const auto refresh = database.refreshFilePresenceForContentHash(contentHash, 16, &error);
    if (!refresh.has_value() || !error.empty()) {
        return finish(false, 32, database.schemaVersion(),
                      refresh ? &*refresh : nullptr, nullptr,
                      error.empty() ? "Fixture-scoped file-presence refresh failed."
                                    : juce::String::fromUTF8(error.c_str()));
    }

    const auto workflow = database.contentHashWorkflowSummary(contentHash, &error);
    if (!workflow.has_value() || !error.empty()) {
        return finish(false, 33, database.schemaVersion(), &*refresh,
                      workflow ? &*workflow : nullptr,
                      error.empty() ? "Fixture workflow summary failed."
                                    : juce::String::fromUTF8(error.c_str()));
    }

    const bool qualified = refresh->complete
        && refresh->unresolved == 0
        && refresh->missing == 0
        && workflow->trackCount >= 2
        && workflow->missingTrackCount == 0
        && workflow->historyCount >= 1
        && workflow->tagAssociationCount >= 1
        && workflow->playlistMembershipCount >= 1;

    if (!qualified) {
        return finish(false, 34, database.schemaVersion(), &*refresh, &*workflow,
                      "Disposable fixture workflow is incomplete: require >=2 connected duplicate rows, >=1 history/tag/playlist association, and zero missing/unresolved fixture rows.");
    }

    return finish(true, 0, database.schemaVersion(), &*refresh, &*workflow);
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
        const bool libraryBackupSmoke = arguments.contains("--library-backup-smoke");
        const bool libraryRestoreSmoke = arguments.contains("--library-restore-smoke");
        if (libraryBackupSmoke || libraryRestoreSmoke) {
            if (libraryBackupSmoke && libraryRestoreSmoke) {
                setApplicationReturnValue(24);
            } else {
                setApplicationReturnValue(runLibraryRecoverySmoke(libraryRestoreSmoke));
            }
            quit();
            return;
        }
        if (arguments.contains("--m4-fixture-state")) {
            setApplicationReturnValue(runM4FixtureStateProbe());
            quit();
            return;
        }
        const bool smokeTest = arguments.contains("--smoke-test");
        const bool keyLockResearch = arguments.contains("--key-lock-research");
        window = std::make_unique<Window>(!smokeTest, keyLockResearch);
        if (smokeTest) {
            guiSmokeSteps.clear();
            guiSmokeWorkstationSteps = 0;
            juce::Timer::callAfterDelay(100, [this] { runGuiSmokeStep(0); });
        }
    }
    void shutdown() override { window.reset(); juce::Logger::setCurrentLogger(nullptr); logger.reset(); }
    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted(const juce::String&) override { if (window) window->toFront(true); }
private:
    class Window final : public juce::DocumentWindow {
    public:
        Window(bool openAudio, bool keyLockResearch)
            : DocumentWindow("BrokeDJ — by Swir", juce::Colour(0xff080e1a), allButtons) {
            setUsingNativeTitleBar(true); setContentOwned(new RecordingMainComponent(openAudio, keyLockResearch), true);
            setResizable(true, false); setResizeLimits(1050, 800, 3840, 2160);
            centreWithSize(getWidth(), getHeight()); setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    void finishGuiSmoke(bool success, const juce::String& error = {}) {
        auto* root = new juce::DynamicObject();
        juce::var report(root);
        root->setProperty("schema_version", 1);
        root->setProperty("mode", "native-resize-lifecycle");
        root->setProperty("plays_audio", false);
        root->setProperty("opens_audio_device", false);
        root->setProperty("success", success);
        root->setProperty("step_count", guiSmokeSteps.size());
        root->setProperty("workstation_step_count", guiSmokeWorkstationSteps);
        root->setProperty("steps", guiSmokeSteps);
        root->setProperty("qualification_note",
                          "No-audio GUI lifecycle/resize and component-geometry evidence only; this does not certify manual usability, HiDPI appearance, audio-device switching, controller input, or live readiness.");
        if (error.isNotEmpty()) root->setProperty("error", oneLine(error));

        const auto output = juce::File::getCurrentWorkingDirectory().getChildFile("BrokeDJ-gui-smoke.json");
        if (!output.replaceWithText(juce::JSON::toString(report, false) + "\n")) {
            setApplicationReturnValue(5);
            juce::Logger::writeToLog("GUI smoke could not write BrokeDJ-gui-smoke.json");
            quit();
            return;
        }

        if (!success) setApplicationReturnValue(4);
        juce::Logger::writeToLog(success ? "GUI smoke resize sequence passed." : "GUI smoke resize sequence failed: " + error);
        quit();
    }

    void runGuiSmokeStep(std::size_t step) {
        static constexpr std::array<std::pair<int, int>, 4> smokeSizes{{
            {1050, 800}, {1280, 860}, {1600, 900}, {1050, 800}
        }};

        if (!window) {
            finishGuiSmoke(false, "Application window disappeared during smoke sequence.");
            return;
        }
        if (step >= smokeSizes.size()) {
            if (guiSmokeWorkstationSteps <= 0) {
                finishGuiSmoke(false, "The deterministic resize sequence never exercised workstation layout.");
                return;
            }
            finishGuiSmoke(true);
            return;
        }

        const auto [requestedWidth, requestedHeight] = smokeSizes[step];
        window->setSize(requestedWidth, requestedHeight);
        auto* content = window->getContentComponent();
        auto* wrapper = dynamic_cast<RecordingMainComponent*>(content);
        auto* workstation = wrapper != nullptr
            ? dynamic_cast<RecordingMainComponentBase*>(wrapper->getChildComponent(0))
            : nullptr;
        const bool workstationLayout = workstation != nullptr && workstation->usingWorkstationLayout();
        const bool geometrySane = workstation != nullptr && workstation->uiGeometrySane();
        if (workstationLayout) ++guiSmokeWorkstationSteps;

        auto* row = new juce::DynamicObject();
        row->setProperty("step", static_cast<int>(step));
        row->setProperty("requested_width", requestedWidth);
        row->setProperty("requested_height", requestedHeight);
        row->setProperty("window_width", window->getWidth());
        row->setProperty("window_height", window->getHeight());
        row->setProperty("content_width", content != nullptr ? content->getWidth() : 0);
        row->setProperty("content_height", content != nullptr ? content->getHeight() : 0);
        row->setProperty("workstation_layout", workstationLayout);
        row->setProperty("geometry_sane", geometrySane);
        guiSmokeSteps.add(juce::var(row));

        const bool acceptedSize = window->getWidth() == requestedWidth && window->getHeight() == requestedHeight;
        const bool contentSane = content != nullptr && content->getWidth() > 0 && content->getHeight() > 0;
        if (!acceptedSize || !contentSane || !geometrySane) {
            finishGuiSmoke(false,
                           geometrySane
                               ? "Native window/content did not accept a deterministic resize target."
                               : "Native workstation component geometry failed containment/overlap validation.");
            return;
        }

        juce::Timer::callAfterDelay(120, [this, next = step + 1] { runGuiSmokeStep(next); });
    }

    juce::Array<juce::var> guiSmokeSteps;
    int guiSmokeWorkstationSteps = 0;
    std::unique_ptr<juce::FileLogger> logger;
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(BrokeDJApplication)
