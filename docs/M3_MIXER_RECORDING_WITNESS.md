# BrokeDJ M3 mixer and recording qualification witness

This is a **user-controlled Windows 11 x64 evidence recorder** for the remaining manual M3 mixer/recording qualification gates. It does not automate audio playback, microphone routing, device switching, volume changes, recording, or listening. It only records a small privacy-minimized JSON witness after a human has performed the checks below.

The witness is bound to the exact `BrokeDJ.exe` by filename, version and SHA-256. It intentionally stores no audio-device names, track names or paths, recording paths, source music, or microphone audio.

## What this witness qualifies

For the current M3 implementation, the manual session reviews:

- the three-band EQ on representative music, including unity/kill behavior and the absence of unexpected bursts or non-finite behavior;
- channel/master gain staging and the visible overload/meter behavior;
- the available crossfader laws during real playback;
- the linked-stereo sample-peak limiter behavior, without claiming true-peak reconstruction, look-ahead transparency or mastering-grade limiting;
- a real microphone input enabled through **MIC I/O**;
- the microphone ducking response and recovery;
- Booth on dedicated logical outputs 5/6 while master remains on 1/2 and private cue remains isolated on 3/4;
- creation and playback review of the post-limiter 24-bit WAV set recording;
- the recording dropout counter after a continuous qualification session;
- runtime/log review for errors that occurred during the session.

A complete witness requires **at least 60 continuous minutes** and **at least three representative user-owned or otherwise licensed tracks**. Sixty minutes is a focused M3 qualification session; it is not the later multi-hour live-soak/release gate.

## Before starting

1. Use the `M3-MIXER-RECORDING-WITNESS.ps1` that was packaged beside the exact `BrokeDJ.exe` you are testing.
2. Start with safe monitor/headphone levels. This guide never asks a script to play sound or change hardware volume.
3. Use only music and microphone material you are allowed to use for testing.
4. For the Booth check, use hardware that can expose at least six independent output channels. If that hardware is unavailable, the M3 physical Booth gate remains open and a complete witness must not be generated.
5. For the microphone checks, enable a real input through **MIC I/O** and verify that disabling/unavailable input does not leak stale input data into master.
6. Keep the BrokeDJ recording-status/dropout result visible when ending the set. Any non-zero recording dropout means the complete witness must be refused until the cause is diagnosed.

## Manual qualification sequence

Perform the following with the same packaged executable:

1. **EQ listening:** review LOW/MID/HIGH on multiple representative sections, return to unity, and exercise full kill. Confirm the behavior is musically usable for this development target and that no unexpected bursts/corruption occur.
2. **Gain staging:** exercise channel trim/master levels and verify meters/overload diagnostics react coherently. Do not use the limiter to hide persistent overload.
3. **Crossfader laws:** exercise the available crossfader curves from both endpoints through the centre during playback and review transitions for unexpected discontinuities.
4. **Limiter:** review limiter enabled/disabled behavior at controlled levels. Confirm bounded operation without claiming true-peak or transparent mastering performance.
5. **Microphone:** enable a real input with **MIC I/O**, then enable MIC and review normal speech/program input through master.
6. **Ducking:** review attenuation and release while microphone input crosses the ducking threshold. Reject the witness for unacceptable clicks, pumping, stuck attenuation or routing errors.
7. **Booth / cue isolation:** with six active outputs, verify protected master on 1/2, private cue on 3/4 and Booth on 5/6. Change Booth level and verify it cannot boost above the protected master. Confirm private cue is not folded into master or Booth.
8. **Recording:** record the continuous qualification set. Confirm a WAV file is created, can be reopened/played back, and represents the expected post-limiter master rather than private cue.
9. **Dropouts:** end the recording and confirm BrokeDJ reports zero recording FIFO dropouts for this qualification run. A non-zero count fails the witness.
10. **Runtime review:** review BrokeDJ's status/log output for errors from the tested workflow. Logs may contain local filenames or device information; do not attach raw logs without reviewing/redacting them.

## Create the witness

From the directory containing the packaged executable and witness script:

```powershell
pwsh -NoProfile -File .\M3-MIXER-RECORDING-WITNESS.ps1 `
  -AppPath .\BrokeDJ.exe `
  -ContinuousSessionMinutes 60 `
  -RepresentativeTrackCount 3 `
  -UserOwnedOrLicensed `
  -EqListeningReviewed `
  -GainStagingReviewed `
  -CrossfaderLawsReviewed `
  -LimiterBehaviorReviewed `
  -MicrophoneInputReviewed `
  -DuckingReviewed `
  -BoothRoutingReviewed `
  -CueIsolationPreserved `
  -RecordingCreated `
  -RecordingPlaybackReviewed `
  -ZeroRecordingDropoutsObserved `
  -RuntimeErrorReview
```

The default evidence file is `BrokeDJ-M3-mixer-recording.json`. The JSON contains only closed, typed qualification fields, environment class, duration/counts and the exact executable fingerprint. It does not contain the names or paths of test tracks, recordings or devices.

## Validate an existing witness

```powershell
pwsh -NoProfile -File .\M3-MIXER-RECORDING-WITNESS.ps1 `
  -AppPath .\BrokeDJ.exe `
  -EvidencePath .\BrokeDJ-M3-mixer-recording.json `
  -ValidateExisting
```

Exit codes:

- `0` — the evidence schema is complete and matches the selected executable;
- `2` — generation wrote an intentionally incomplete record because one or more acceptance fields were not satisfied;
- `1` — malformed/unsafe/mismatched evidence, unsupported environment, missing file, or CI-generation refusal.

## CI safety and privacy contract

CI is allowed to parse the script and validate synthetic fixtures. **CI generation mode is deliberately refused**, even when every command-line switch is supplied. Therefore a green workflow cannot manufacture a claim that a human listened to or physically tested the mixer.

Validation rejects:

- Windows builds older than the Windows 11 class or non-x64 evidence;
- fewer than 60 continuous minutes or fewer than three representative tracks;
- material that is not attested user-owned/licensed;
- false or type-spoofed acceptance fields;
- privacy flags indicating device names, local paths, track names, recordings, source music or microphone audio were embedded;
- unexpected/free-form fields;
- evidence whose executable fingerprint does not match the selected `BrokeDJ.exe`.

## What this still does not prove

A valid witness does **not** certify zero latency, ASIO support, true-peak limiting, inaudible DSP, compatibility with every microphone/interface, controller support, multi-hour live reliability or public-release readiness. It also does not close M1/M2/M4 hardware/listening gates. M3 may be marked complete only after this evidence is reviewed together with the milestone's existing deterministic tests and any regression exposed by the real session is fixed.

See also [`EQ_VALIDATION.md`](EQ_VALIDATION.md), [`TESTING.md`](TESTING.md) and the M3 row in [`../ROADMAP.md`](../ROADMAP.md).
