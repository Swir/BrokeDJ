# M1 Windows hardware qualification witness

This document is a **manual evidence procedure**, not a claim that M1 has passed. It is designed to make the remaining Windows 11 audio-device gate reproducible without running loud tests automatically.

## Scope

M1 still requires all of the following on a real Windows 11 x64 machine:

- clean-machine launch of the staged/portable build;
- resize/usability review at the supported window limits and normal HiDPI scaling;
- local-file import and playback through a real output device;
- device switching and recovery without a crash or stale private cue route;
- independent master 1/2 and cue 3/4 on hardware that genuinely exposes four output channels;
- observation of device/runtime errors or xruns when the backend reports them.

CI compilation, no-audio GUI smoke, offline routing tests and the silent probe below are prerequisites only. They do not close M1.

## 1. Silent capability inventory

Before connecting headphones or raising monitor volume, run from the folder containing the built executable:

```powershell
.\BrokeDJ.exe --device-probe
Get-Content .\BrokeDJ-device-probe.txt
$probe = Get-Content .\BrokeDJ-device-probe.json -Raw | ConvertFrom-Json
$probe | Format-List schema_version,mode,backend_count,output_device_count,four_output_candidate_count,safety_invariants_ok
```

The command writes a human-readable TXT report and a schema-versioned JSON report in the current working directory. The probe does **not** call `AudioIODevice::open`, start an audio callback or emit audio. The full local mode constructs JUCE device descriptors only to query their advertised capability lists.

Before using the report as evidence, verify:

- `schema_version` matches the documented probe contract;
- `calls_device_open=false`;
- `starts_audio_callback=false`;
- `unexpected_open_state_count=0`;
- `safety_invariants_ok=true`.

The probe checks `AudioIODevice::isOpen()` before and after capability queries and returns non-zero if an unexpectedly open descriptor is observed. This is a guard on the diagnostic path, not a qualification of the driver.

A device marked `four_output_candidate=true` in JSON (`four_output_candidate=yes` in TXT) merely advertises at least four output channels through the driver descriptor. It does not prove that outputs 3/4 are physically independent or that the selected driver works correctly under load.

Review both reports before sharing them because device names may identify the local hardware setup. A useful private witness record can include the SHA-256 of the JSON file instead of publishing the full hardware inventory:

```powershell
Get-FileHash .\BrokeDJ-device-probe.json -Algorithm SHA256
```

## 2. Clean launch and resize witness

1. Use a fresh staged/portable copy rather than the developer build tree.
2. Start `BrokeDJ.exe` normally.
3. Verify the icon/window title and that all four decks are visible without controls overlapping.
4. Resize to the minimum supported window (`1050 × 800`), then to a normal desktop size such as `1440 × 900` or larger.
5. Repeat at the Windows display scale(s) that are actually being qualified, recording the scale and resolution.
6. Verify the denser Beat Loop / Hot Cue / Beat Jump / MASTER-SYNC rows remain usable.

Record only what was actually observed. A successful CI smoke launch is not a substitute for this step.

## 3. Import and ordinary playback witness

Use a local track that you have the right to test. Start with low hardware volume.

1. Load the file by the Load button, then repeat with drag-and-drop.
2. Confirm playback starts before background musical analysis is required to finish.
3. Exercise play/pause, waveform seek and CUE 0.
4. Exercise whole-track loop separately from reviewed-grid Beat Loop.
5. If a reviewed grid is available, exercise Hot Cue, Beat Jump and one-shot MASTER/SYNC while confirming failed/out-of-range actions do not destabilize playback.
6. Note codec, sample rate, device/backend and buffer size in the private test record; do not publish copyrighted audio or private file paths.

## 4. Device switching witness

With monitor/headphone volume low:

1. Open Audio settings and select the first intended output device.
2. Start ordinary playback and verify expected master output.
3. Stop playback before changing a driver/backend when the device requires it.
4. Switch to the second intended device and confirm BrokeDJ remains responsive and playback can be restarted.
5. Switch back once and repeat a seek/play/pause sequence.
6. If a device disappears or the switch fails, record the exact backend/device state and whether BrokeDJ recovered without a crash.

This step is intentionally manual because OS/driver behavior cannot be established by a hosted CI runner.

## 5. Four-output cue witness

Only perform this on an interface that genuinely exposes two independent stereo output pairs. Keep hardware levels low before enabling playback.

1. Configure four output channels in Audio settings.
2. Route logical outputs 1/2 to the master/monitor pair and 3/4 to headphones or a separate safe destination.
3. With channel gain down, enable CUE on one deck and verify private cue is audible only on 3/4.
4. Verify the same cue is not folded into master 1/2.
5. Raise the channel/master path and verify the master remains on 1/2 while CUE stays independently available on 3/4.
6. Repeat for at least two decks and after one device-settings reopen/switch cycle.

Do not claim this gate from a two-output device; BrokeDJ deliberately does not fold private cue into the master.

## Evidence record

A useful private witness note contains:

- BrokeDJ commit SHA and artifact/checksum;
- Windows 11 build and display scale;
- backend and device name from the local probe;
- device-probe JSON schema version and SHA-256;
- advertised and actually verified output-channel count;
- selected sample rate and buffer size;
- clean-launch, resize, import, switching and 4-output cue results;
- any xrun/driver error evidence actually exposed by the backend;
- tester date and concise known issues.

Do not commit local music, private paths, personal machine identifiers or a device report that has not been reviewed for sensitive information.

## Pass rule

M1 may be marked complete only after the repository's stated acceptance criteria are satisfied with real Windows 11 evidence. A successful silent probe, green Windows CI, or a four-channel capability descriptor alone is **not** sufficient.
