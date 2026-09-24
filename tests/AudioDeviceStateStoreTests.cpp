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
    source.setAttribute("audioDeviceName", "PRIVATE LEGACY DEVICE MUST NOT PERSIST");
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
    expect(!raw.contains("PRIVATE LEGACY"), "legacy combined device identity must be stripped");
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
        expect(!loaded->hasAttribute("audioDeviceName"),
               "loaded state must not expose legacy combined-device identity");
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
    expect(AudioDeviceStateStore::requestedOutputChannels(sparse, 4) == 2,
           "non-contiguous physical outputs must be counted by active channels, not highest bit");

    juce::XmlElement highQuad("DEVICESETUP");
    highQuad.setAttribute("audioDeviceOutChans", "11110000");
    expect(AudioDeviceStateStore::requestedOutputChannels(highQuad, 2) == 4,
           "four selected physical outputs must restore as four logical channels");

    juce::XmlElement oneChannel("DEVICESETUP");
    oneChannel.setAttribute("audioDeviceOutChans", "1");
    expect(AudioDeviceStateStore::requestedOutputChannels(oneChannel, 3) == 3,
           "mono explicit output must fail closed to the bounded caller fallback");

    juce::XmlElement tooMany("DEVICESETUP");
    tooMany.setAttribute("audioDeviceOutChans", "11111");
    expect(AudioDeviceStateStore::requestedOutputChannels(tooMany, 2) == 2,
           "more than four explicit outputs must fail closed to the bounded fallback");

    juce::XmlElement malformedMask("DEVICESETUP");
    malformedMask.setAttribute("audioDeviceOutChans", "11oops");
    expect(AudioDeviceStateStore::requestedOutputChannels(malformedMask, 4) == 4,
           "malformed output masks must fail closed instead of being partially parsed");

    juce::XmlElement noMask("DEVICESETUP");
    expect(AudioDeviceStateStore::requestedOutputChannels(noMask, 3) == 3,
           "missing channel mask must keep bounded caller fallback");

    juce::XmlElement unsafe("DEVICESETUP");
    unsafe.setAttribute("audioOutputDeviceName", "Unsafe Fixture");
    unsafe.setAttribute("audioDeviceOutChans", "11111");
    unsafe.setAttribute("audioDeviceRate", -1.0);
    unsafe.setAttribute("audioDeviceBufferSize", -64);
    expect(store.store(unsafe), "unsafe optional fields must be sanitised rather than corrupting state");
    loaded = store.load();
    expect(loaded != nullptr, "sanitised unsafe state must remain loadable");
    if (loaded) {
        expect(!loaded->hasAttribute("audioDeviceOutChans"),
               "unsupported >4-channel mask must not survive sanitisation");
        expect(!loaded->hasAttribute("audioDeviceRate"),
               "non-positive sample rate must not survive sanitisation");
        expect(!loaded->hasAttribute("audioDeviceBufferSize"),
               "non-positive buffer size must not survive sanitisation");
    }

    juce::XmlElement sparseStored("DEVICESETUP");
    sparseStored.setAttribute("audioOutputDeviceName", "Sparse Fixture");
    sparseStored.setAttribute("audioDeviceOutChans", "1001");
    expect(store.store(sparseStored), "valid sparse physical output selection must persist");
    loaded = store.load();
    expect(loaded != nullptr, "sparse output state must load");
    if (loaded) {
        expect(loaded->getStringAttribute("audioDeviceOutChans") == "1001",
               "valid physical output mask must be preserved exactly");
        expect(AudioDeviceStateStore::requestedOutputChannels(*loaded, 4) == 2,
               "sparse two-output state must request two logical channels");
    }

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
