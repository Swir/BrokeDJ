# M1 Windows hardware qualification witness

This document is a **manual evidence procedure**, not a claim that M1 has passed. It makes the remaining Windows 11 audio-device gate reproducible without automating loud tests or claiming hardware behavior that has not been observed.

Current Windows development artifacts place `M1-HARDWARE-WITNESS.ps1` and this guide beside `BrokeDJ.exe`, and the package manifest/checksum contract covers both files. The recorder binds accepted evidence to the exact staged executable and the exact silent device-probe JSON used for the qualification attempt.

## Scope

M1 still requires all of the following on a real Windows 11 x64 machine:

- clean-machine launch of the staged/portable build;
- resize/usability review at the supported window limits and normal HiDPI scaling;
- import and playback through a real output device;
- device switching and recovery without a crash or stale private cue route;
- fail-safe handling of a selected output becoming unavailable: playback pauses, recovery does not auto-resume, and the operator explicitly resumes;
- persistence of the selected output backend/device, sample rate, buffer and 2–4 output-channel selection across a normal app restart when that setup is still available;
- independent master 1/2 and cue 3/4 on hardware that genuinely exposes four output channels;
- review of device/runtime errors or xruns when the backend reports them.

CI compilation, no-audio GUI smoke, offline routing tests and the silent probe below are prerequisites only. They do not close M1.

## Safety, privacy and the disposable playback fixture

The witness prepares its own disposable synthetic playback file, so personal music is not required for the M1 import/playback step. The fixture is a quiet generated 440 Hz, 48 kHz stereo, 16-bit PCM WAV with short fades at both ends. The script **never starts playback**, opens a playback device on behalf of the tester, disconnects hardware, changes device settings, restarts BrokeDJ, or changes hardware volume. Keep monitor/headphone volume conservative before pressing Play yourself.

The fixture exists only for the current witness run and is removed when the script exits. Its local path is printed for operator guidance but is never written into accepted M1 evidence. The internal `-FixtureSelfTest` path creates, structurally validates and removes the WAV without resolving or launching `AppPath`; CI uses that path only to test the fixture contract, not to create human evidence.

Evidence generation is rejected before resolving, hashing or launching `AppPath` when either `CI` or `GITHUB_ACTIONS` has a common truthy value (`1`, `true`, `yes`, `on`, case-insensitive). `-ValidateExisting` remains available in CI because it only checks an already-created record.

A failed or interrupted new witness never deliberately replaces an earlier accepted evidence file. All manual checks must pass before a candidate is serialized. The candidate is written to a sibling temporary file, fully validated against the exact executable and probe fingerprints, then moved into the requested evidence path. Temporary candidates and the disposable playback fixture are cleaned up on exit.

M1 evidence uses **schema 2**. Schema 1 evidence is deliberately rejected because it predates the explicit restart-persistence and device-loss/no-auto-resume attestations required by the current first-Beta gate.

## Recommended staged-artifact workflow

Extract the Windows development artifact, open PowerShell in the `BrokeDJ` directory and run:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\M1-HARDWARE-WITNESS.ps1 -AppPath ".\BrokeDJ.exe"
```

The recorder first runs BrokeDJ's **silent** `--device-probe` mode in the current directory. That mode does not call `AudioIODevice::open`, start an audio callback or play audio. The script validates the probe's schema/safety invariants and requires at least one output device plus at least one descriptor that advertises four output channels before it will proceed to the manual M1 checks.

It then creates and validates the disposable playback fixture and prints its local path. The recorder does **not** automate playback, device switching, device loss, app restart, cue routing or volume changes. It prompts the tester only after the silent probe and fixture preflight have passed.

It writes or updates these local files:

- `BrokeDJ-device-probe.json` — detailed local capability inventory; this can include backend/device names and should be treated as private until reviewed;
- `BrokeDJ-M1-Hardware-Witness.json` — closed-schema witness data containing only Windows/app identity, probe filename/hash/counts, pass/fail booleans and explicit privacy flags.

The accepted witness JSON deliberately does not contain device names, track names, track paths, screenshots or source music. Validation rejects unknown/free-form fields so private notes cannot silently become part of accepted evidence.

When working from a source checkout instead of the staged artifact:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\m1_hardware_witness.ps1 `
  -AppPath ".\build\windows\BrokeDJ_artefacts\Release\BrokeDJ.exe"
```

A `no` answer is a real M1 blocker. The script stops before publishing new evidence. Fix the product/hardware setup or repeat the witness after the issue is understood; do not edit failed checks into success.

## 1. Silent capability inventory

The recorder runs the equivalent of:

```powershell
.\BrokeDJ.exe --device-probe
Get-Content .\BrokeDJ-device-probe.txt
$probe = Get-Content .\BrokeDJ-device-probe.json -Raw | ConvertFrom-Json
$probe | Format-List schema_version,mode,backend_count,output_device_count,four_output_candidate_count,safety_invariants_ok
```

The command writes a human-readable TXT report and a schema-versioned JSON report in the working directory. The full local mode constructs JUCE device descriptors only to query their advertised capability lists.

Before using the report as evidence, the recorder verifies:

- `schema_version=2`;
- `mode=silent-capability`;
- `plays_audio=false`;
- `calls_device_open=false`;
- `starts_audio_callback=false`;
- `creates_device_descriptors=true`;
- `unexpected_open_state_count=0`;
- `safety_invariants_ok=true`;
- at least one output device is reported;
- at least one `four_output_candidate` is reported.

A `four_output_candidate` merely advertises at least four output channels through a driver descriptor. It does **not** prove that outputs 3/4 are physically independent, that device switching/loss works, that restart persistence works, or that the driver is stable under load. Those remain manual M1 checks.

The witness JSON stores the SHA-256 of the exact probe JSON. Revalidation therefore fails if the detailed local probe is changed or swapped after the witness was recorded.

## 2. Clean launch and resize witness

1. Use a fresh staged/portable copy rather than the developer build tree.
2. Start `BrokeDJ.exe` normally.
3. Verify the icon/window title and that all four decks are visible without controls overlapping.
4. Resize to the minimum supported window (`1050 × 800`), then to a normal desktop size such as `1440 × 900` or larger.
5. Repeat at the Windows display scale(s) that are actually being qualified.
6. Verify the denser Beat Loop / Hot Cue / Beat Jump / MASTER-SYNC rows remain usable.

The recorder's `cleanLaunch` and `resizeAndHiDpi` answers must reflect what was actually observed. A successful CI smoke launch is not a substitute.

## 3. Import and ordinary playback witness

Use the generated `BrokeDJ-M1-Playback-Fixture.wav` whose temporary path is printed by the witness. Start with low hardware volume.

1. Load the generated file by the Load button, then repeat with drag-and-drop.
2. Confirm playback starts before background musical analysis is required to finish.
3. Exercise play/pause, waveform seek and CUE 0.
4. Exercise whole-track loop separately from reviewed-grid Beat Loop.
5. If a reviewed grid is available, exercise Hot Cue, Beat Jump and one-shot MASTER/SYNC while confirming failed/out-of-range actions do not destabilize playback.
6. Keep any backend/buffer-size notes private unless explicitly reviewed for sharing.

The generated WAV is only a controlled import/playback fixture. It does not replace representative-music BPM/key or key-lock listening evidence required by M2. The recorder stores only the boolean `importAndPlayback` result, not the temporary fixture path.

## 4. Device switching and loss witness

With monitor/headphone volume low:

1. Open Audio settings and select the first intended output device.
2. Start ordinary playback of the generated fixture and verify expected master output.
3. Stop playback before changing a driver/backend when the device requires it.
4. Switch to the second intended device and confirm BrokeDJ remains responsive and playback can be restarted.
5. Switch back once and repeat a seek/play/pause sequence.
6. Start playback again at safe volume, then make the selected output unavailable using a normal reversible hardware/Windows action appropriate for that device (for example disconnecting a USB interface or disabling that output device).
7. Confirm BrokeDJ remains responsive and playback is paused rather than continuing against stale routing.
8. Restore the device and confirm playback **does not automatically resume**; verify the routing first, then press Play yourself and confirm ordinary playback can resume.
9. If the device disappears, recovery behaves differently, or the switch fails, record the issue privately and answer the corresponding witness check `no` until recovery is understood.

The two independent schema-2 booleans are `deviceSwitchRecovery` and `deviceLossFailsSafe`. This step is intentionally manual because OS/driver and physical disconnect behavior cannot be established by a hosted CI runner.

## 5. Output-settings restart persistence witness

This step qualifies the output-only persistence introduced for the first-Beta path. It is not satisfied by reopening the Audio settings dialog; perform an actual normal app restart.

1. Choose the intended output backend/device and a supported sample rate/buffer size.
2. Select either two outputs for master-only operation or four outputs when qualifying independent Cue 3/4.
3. Close BrokeDJ normally so its bounded local output state is persisted.
4. Reopen the **same staged candidate** without manually reselecting the device.
5. Verify the same available backend/device, sample rate, buffer and explicit 2–4 output-channel selection are restored.
6. Verify BrokeDJ has not restored an input device or MIDI state as part of this output-only persistence boundary.
7. For a four-output candidate, continue directly into the cue-isolation check below so the restored channel selection is proven physically, not merely displayed.

If Windows or the driver makes the saved setup unavailable, BrokeDJ is expected to fail closed to a usable current/default setup rather than pretending the old route was restored. That case is useful recovery evidence but does not satisfy `deviceSettingsPersistAcrossRestart=yes` for the intended available setup; repeat when the target setup is genuinely available.

## 6. Four-output cue witness

Only perform this on an interface that genuinely exposes two independent stereo output pairs. Keep hardware levels low before enabling playback.

1. Configure four output channels in Audio settings.
2. Route logical outputs 1/2 to the master/monitor pair and 3/4 to headphones or a separate safe destination.
3. With channel gain down, enable CUE on one deck and verify private cue is audible only on 3/4.
4. Verify the same cue is not folded into master 1/2.
5. Raise the channel/master path and verify the master remains on 1/2 while CUE stays independently available on 3/4.
6. Repeat for at least two decks and after the restart/settings-switch checks above.

Do not claim this gate from a two-output device. BrokeDJ deliberately does not fold private cue into the master. The recorder requires both a four-output candidate in the silent probe and an affirmative manual `fourOutputCueIsolation` check; the candidate alone is never treated as proof.

## 7. Runtime/driver error review

Review any driver/runtime/xrun information that the selected backend actually exposes during the witness. The recorder does not invent a zero-xrun claim when the backend does not provide that metric. Answer `runtimeErrorReview=yes` only when no unresolved M1-blocking device/runtime error remains from the observed workflow.

## Validate saved evidence

For a staged artifact:

```powershell
.\M1-HARDWARE-WITNESS.ps1 `
  -AppPath ".\BrokeDJ.exe" `
  -ProbePath ".\BrokeDJ-device-probe.json" `
  -EvidencePath ".\BrokeDJ-M1-Hardware-Witness.json" `
  -ValidateExisting
```

For a source checkout use `scripts\m1_hardware_witness.ps1` with the same parameters.

Validation recomputes the executable filename/version/SHA-256 and the exact probe SHA-256, requires **M1 schema 2**, type-checks the closed evidence schema, confirms Windows 11 x64 evidence, revalidates the silent probe safety contract and requires every M1 manual check to remain a real JSON boolean `true`. It rejects schema-1 evidence, missing restart/loss checks, extra evidence fields, wrong app/probe fingerprints, privacy flags that claim private data was captured, zero four-output candidates and unsafe/malformed probe data.

## Evidence handling

A useful **private** qualification package can retain:

- `BrokeDJ-M1-Hardware-Witness.json`;
- `BrokeDJ-device-probe.json` and its SHA-256;
- BrokeDJ package manifest/checksums and `SOURCE-COMMIT.txt`;
- private notes for display scale, backend/device identity, selected sample rate/buffer size, the reversible device-loss action used, restart outcome and any runtime/xrun diagnostics actually exposed by the driver.

Do not commit private file paths, screenshots with personal information or an unreviewed detailed device probe. The small witness JSON is privacy-minimized, but the detailed probe can identify the local hardware setup. The generated playback fixture is disposable and is removed by the witness; it is not part of the evidence package.

## Pass rule

M1 may be marked complete only after the repository's stated acceptance criteria are satisfied with real Windows 11 evidence. A successful synthetic-fixture self-test, silent probe, green Windows CI, a four-channel capability descriptor or a generated witness file alone is **not** sufficient. The actual launch/resize/import/device-switch/device-loss/restart-persistence/four-output-cue workflow must be performed and reviewed on real hardware.
