// SPDX-License-Identifier: AGPL-3.0-only
#include "app/PersistentAudioAppComponent.h"

#include <iostream>

namespace {
int failures = 0;

void expect(bool condition, const char* message) {
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}

juce::File makeTempStateFile() {
    const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("BrokeDJ-AudioDeviceStateStoreTests-" + juce::Uuid().toString());
    root.createDirectory();
    return root.getChildFile("audio-device.xml");
}
}

int main() {
    using broke::app::AudioDeviceStateStore;

    const auto stateFile = makeTempStateFile();
    const auto root = stateFile.getParentDirectory();
    AudioDeviceStateStore store(stateFile);

    expect(!store.load(), "missing state must not restore");

    juce::XmlElement source("DEVICESETUP");
    source.setAttribute("deviceType", "Fixture Backend");
    source.setAttribute("audioOutputDeviceName", "Fixture Output");
    source.setAttribute("audioInputDeviceName", "PRIVATE MIC MUST NOT PERSIST");
    source.setAttribute("audioDeviceRate", 48000.0);
    source.setAttribute("audioDeviceBufferSize", 256);
    source.setAttribute("audioDeviceOutChans", "1111");
    auto* midi = source.createNewChildElement("MIDIINPUT");
    midi->setAttribute("name", "PRIVATE MIDI MUST NOT PERSIST");

    expect(store.store(source), "valid JUCE device state must store transactionally");
    expect(stateFile.existsAsFile(), "stored state file must exist");
    expect(stateFile.getSize() > 0 && stateFile.getSize() <= AudioDeviceStateStore::maxStateBytes,
           "stored state must remain bounded");

    const auto raw = stateFile.loadFileAsString();
    expect(raw.contains("Fixture Output"), "output identity must be retained locally");
    expect(!raw.contains("PRIVATE MIC"), "input-device identity must be stripped");
    expect(!raw.contains("PRIVATE MIDI"), "MIDI identity must be stripped");
    expect(!raw.contains("MIDIINPUT"), "MIDI child state must be stripped");
    expect(raw.contains("brokedjStateVersion=\"1\""), "state schema marker must be written");

    auto loaded = store.load();
    expect(loaded != nullptr, "stored state must round-trip");
    if (loaded) {
        expect(loaded->hasTagName("DEVICESETUP"), "round-trip root must remain DEVICESETUP");
        expect(loaded->getStringAttribute("audioOutputDeviceName") == "Fixture Output",
               "round-trip output identity mismatch");
        expect(loaded->getDoubleAttribute("audioDeviceRate") == 48000.0,
               "round-trip sample rate mismatch");
        expect(loaded->getIntAttribute("audioDeviceBufferSize") == 256,
               "round-trip buffer mismatch");
        expect(AudioDeviceStateStore::requestedOutputChannels(*loaded, 2) == 4,
               "four-output cue layout must request four channels on restore");
        expect(!loaded->hasAttribute("audioInputDeviceName"),
               "loaded state must never expose stripped input identity");
    }

    juce::XmlElement stereo("DEVICESETUP");
    stereo.setAttribute("audioOutputDeviceName", "Stereo Fixture");
    stereo.setAttribute("audioDeviceOutChans", "11");
    expect(store.store(stereo), "second valid state must replace the first");
    loaded = store.load();
    expect(loaded != nullptr, "replacement state must load");
    if (loaded) {
        expect(loaded->getStringAttribute("audioOutputDeviceName") == "Stereo Fixture",
               "replacement must not retain previous output identity");
        expect(AudioDeviceStateStore::requestedOutputChannels(*loaded, 4) == 2,
               "stereo channel mask must restore as two outputs");
    }

    juce::XmlElement sparse("DEVICESETUP");
    sparse.setAttribute("audioDeviceOutChans", "1001");
    expect(AudioDeviceStateStore::requestedOutputChannels(sparse, 2) == 4,
           "sparse channel 4 selection must retain a four-channel maximum");

    juce::XmlElement noMask("DEVICESETUP");
    expect(AudioDeviceStateStore::requestedOutputChannels(noMask, 3) == 3,
           "missing channel mask must keep bounded caller fallback");

    expect(stateFile.replaceWithText("<DEVICESETUP audioOutputDeviceName=\"x\"/>"),
           "schema-less fixture write failed");
    expect(!store.load(), "state without BrokeDJ schema marker must fail closed");

    expect(stateFile.replaceWithText("<NOTDEVICE brokedjStateVersion=\"1\"/>"),
           "wrong-root fixture write failed");
    expect(!store.load(), "wrong XML root must fail closed");

    expect(stateFile.replaceWithText("not xml"), "malformed fixture write failed");
    expect(!store.load(), "malformed XML must fail closed");

    juce::String oversized;
    oversized.preallocateBytes(static_cast<size_t>(AudioDeviceStateStore::maxStateBytes + 1024));
    while (oversized.getNumBytesAsUTF8() <= AudioDeviceStateStore::maxStateBytes)
        oversized += "0123456789abcdef";
    expect(stateFile.replaceWithText(oversized), "oversized fixture write failed");
    expect(!store.load(), "oversized state must fail closed before XML parsing");

    expect(store.clear(), "clear must remove state safely");
    expect(!stateFile.existsAsFile(), "clear must remove state file");
    expect(store.clear(), "clear must be idempotent");

    root.deleteRecursively();
    if (failures != 0) {
        std::cerr << failures << " AudioDeviceStateStore test(s) failed\n";
        return 1;
    }
    std::cout << "AudioDeviceStateStore tests passed\n";
    return 0;
}
