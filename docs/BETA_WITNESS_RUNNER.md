# First-Beta witness runner

`beta_witness_runner.ps1` is a human-controlled Windows 11 x64 coordinator for BrokeDJ's existing M1–M4 witness tools. It does not replace any milestone validator, does not fabricate hardware/listening evidence and does not publish a GitHub Release. Its job is to keep one exact candidate executable and one evidence directory bound together while the remaining manual Beta gates are completed.

The runner is intended for the current **first usable Windows 11 x64 Beta** qualification scope. M5+ remains outside this flow.

## Safety and evidence boundary

The runner deliberately keeps audio decisions with the tester:

- it may run `BrokeDJ.exe --device-probe`, which is the existing silent capability probe and does not call `AudioIODevice::open`, start the audio callback or play audio;
- it delegates M1 and M4 to their existing interactive witness scripts;
- it **does not** start ordinary playback, key-lock listening, microphone input or recording for M2/M3;
- before M2/M3 evidence can be generated, the tester must complete the documented procedure and explicitly attest every required check;
- every generated witness is revalidated against the exact selected `BrokeDJ.exe`;
- final `BrokeDJ-Beta-Qualification.json` is created only by the existing `BETA-QUALIFICATION.ps1` after M1–M4 all validate;
- generation is refused in CI before `AppPath` is resolved or hashed.

A completed runner session is not a claim of stable/live-ready software. Public Beta publication still requires the repository's separate exact-head CI, package/source/notices/checksum and known-issues gates.

## Requirements

Use Windows 11 x64 with x64 PowerShell. Keep the smoke-qualified BrokeDJ Beta Preview candidate extracted in a writable local folder. The selected build must contain the matching witness scripts staged beside `BrokeDJ.exe`, or run the runner from a source checkout at the same commit so it can resolve the repository copies under `scripts/`.

Do not mix witness files from different BrokeDJ executables. Existing evidence is validated against the selected executable and is reused only when its fingerprint matches.

## Recommended layout

For a downloaded candidate, a convenient layout is:

```text
C:\BrokeDJ-Beta-Test\
  BrokeDJ\
    BrokeDJ.exe
    SOURCE-COMMIT.txt
    M1-HARDWARE-WITNESS.ps1
    M2-KEYLOCK-LISTENING-WITNESS.ps1
    M3-MIXER-RECORDING-WITNESS.ps1
    M4-LIBRARY-WITNESS.ps1
    BETA-QUALIFICATION.ps1
    ...
  evidence\
```

The evidence directory is local test state. Review it before sharing. The witness schemas are intentionally privacy-minimized and do not contain music files.

## Run the full manual sequence

From the matching source checkout:

```powershell
pwsh -NoProfile -File .\scripts\beta_witness_runner.ps1 `
  -AppPath 'C:\BrokeDJ-Beta-Test\BrokeDJ\BrokeDJ.exe' `
  -EvidenceDirectory 'C:\BrokeDJ-Beta-Test\evidence'
```

Windows PowerShell 5.1 is also supported:

```powershell
powershell.exe -NoProfile -File .\scripts\beta_witness_runner.ps1 `
  -AppPath 'C:\BrokeDJ-Beta-Test\BrokeDJ\BrokeDJ.exe' `
  -EvidenceDirectory 'C:\BrokeDJ-Beta-Test\evidence'
```

The sequence is:

1. create or reuse a valid silent full device probe;
2. validate an existing M1 witness or enter the existing M1 hardware witness;
3. validate an existing M2 witness or stop for the documented baseline/key-lock listening review, then collect the exact required attestations;
4. validate an existing M3 witness or stop for the documented mixer/microphone/recording session, then collect the exact required attestations;
5. validate an existing M4 witness or enter the exact-process-bound library/session witness;
6. invoke the existing Beta qualifier and immediately revalidate its output.

If an existing witness belongs to another executable or fails its schema, the runner does not silently count it as passed. The appropriate witness must be completed again against the selected candidate.

## Check status without generating evidence

`-StatusOnly` never creates witness evidence. It validates whatever is present and exits `0` only when the composed qualification already validates. Incomplete status exits `3`; malformed inputs or unsupported environment fail with `1`.

```powershell
pwsh -NoProfile -File .\scripts\beta_witness_runner.ps1 `
  -AppPath 'C:\BrokeDJ-Beta-Test\BrokeDJ\BrokeDJ.exe' `
  -EvidenceDirectory 'C:\BrokeDJ-Beta-Test\evidence' `
  -StatusOnly
```

The summary line reports only booleans for Probe/M1/M2/M3/M4/Qualification. It does not print track names, device names or local music paths.

## Revalidate a completed qualification

`-ValidateExisting` calls the canonical Beta qualifier in validation mode. That qualifier in turn reruns all M1–M4 validators and checks the exact executable, `SOURCE-COMMIT.txt`, the probe and evidence SHA-256 values.

```powershell
pwsh -NoProfile -File .\scripts\beta_witness_runner.ps1 `
  -AppPath 'C:\BrokeDJ-Beta-Test\BrokeDJ\BrokeDJ.exe' `
  -EvidenceDirectory 'C:\BrokeDJ-Beta-Test\evidence' `
  -ValidateExisting
```

## CI self-test boundary

The runner contains a prompt-free `-FixtureSelfTest` used only to verify its fixed file-name/tool-resolution and CI-environment parsing logic. Repository CI also launches a separate child process with `CI=true` and a deliberately missing `AppPath`; the expected failure must be the CI-generation refusal, proving that unattended generation is rejected before a candidate executable can be resolved or hashed.

Those checks validate orchestration behavior only. They cannot substitute for a Windows 11 tester, physical audio outputs, representative listening material, microphone/recording review or the connected M4 library/session procedure.
