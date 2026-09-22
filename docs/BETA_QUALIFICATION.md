# BrokeDJ first Windows Beta qualification

The current delivery target is the **first usable Windows 11 x64 Beta**. This gate does not widen the product roadmap: it joins the already-defined M1–M4 manual evidence into one exact-executable qualification record so the project can move from development artifacts to a real Beta candidate without inventing hardware or listening results.

## What this gate proves

`BETA-QUALIFICATION.ps1` accepts only evidence that the existing M1, M2, M3 and M4 witness validators accept against the **same `BrokeDJ.exe`**. It also binds the result to `SOURCE-COMMIT.txt`, the exact executable SHA-256 and the SHA-256 of every input evidence file.

A successful summary means the candidate has the required manual Windows 11 evidence for the currently implemented M1–M4 core workflow. It does **not** by itself claim controller compatibility, arbitrary third-party hardware compatibility, zero latency, transparent DSP, live-readiness, a stable release, or completion of M5–M9.

## Required files

Run from the staged BrokeDJ folder containing the exact executable and the packaged witness tools. Keep the following evidence files beside it unless explicit paths are supplied:

- `BrokeDJ-device-probe.json` from the full silent-capability probe used by M1;
- `BrokeDJ-M1-Hardware-Witness.json`;
- `BrokeDJ-M2-keylock-listening.json`;
- `BrokeDJ-M3-mixer-recording.json`;
- `BrokeDJ-M4-Library-Witness.json`;
- `SOURCE-COMMIT.txt` from the same staged package.

The four milestone evidence files must already have been produced by their packaged, human-controlled witness procedures on Windows 11 x64. The Beta orchestrator re-runs every validator before it writes anything.

## Generate the Beta qualification summary

From PowerShell 7 in the staged BrokeDJ directory:

```powershell
pwsh -NoProfile -File .\BETA-QUALIFICATION.ps1
```

On success this writes `BrokeDJ-Beta-Qualification.json`. The summary intentionally stores no device names, track names, local paths, recording paths, source music or microphone audio. It contains only source/application identity, pass/fail gate booleans and file names plus SHA-256 fingerprints.

Generation refuses CI. Automated jobs may verify packaging and negative/refusal behavior, but they cannot mint human listening/hardware qualification.

## Revalidate an existing summary

```powershell
pwsh -NoProfile -File .\BETA-QUALIFICATION.ps1 -ValidateExisting
```

Revalidation fails if the executable, source commit, device probe or any M1–M4 evidence file has changed, if any individual witness no longer validates, if the summary contains unexpected fields, or if its privacy contract is altered.

## Beta publication gate

A public Beta still requires the repository's normal exact-head CI, packaged-EXE smoke, source/notices/checksum integrity and review of known issues in addition to this manual qualification summary. Do not create a public Beta release merely because this JSON exists; it is one release-gate input tied to one exact candidate executable.
