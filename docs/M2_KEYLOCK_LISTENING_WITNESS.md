# M2 key-lock listening witness

This document defines a **manual, user-controlled Windows 11 x64 listening witness** for BrokeDJ's opt-in M2 key-lock research path. It records a privacy-minimized result for one exact `BrokeDJ.exe`; it does not automate playback, select files, change volume, certify hardware latency or claim that key lock is production-ready.

The research path remains compiled only when `BROKEDJ_BUILD_TIMESTRETCH_PROTOTYPE=ON` and is entered with `--key-lock-research`. Ordinary BrokeDJ playback remains the fallback and does not depend on the optional processor.

## What this witness is for

Automated CTest already checks bounded time/pitch behavior, fallback semantics and warmed zero-heap processing. Those tests cannot establish whether representative music sounds acceptable. This witness makes the remaining human listening evidence reproducible without committing music or private file paths.

A valid witness requires:

- Windows 11 x64 and x64 PowerShell;
- the exact `BrokeDJ.exe` fingerprint stored in the evidence;
- at least **three** representative tracks that you own or are licensed to use for testing;
- a normal-playback baseline review;
- key-lock review at a slower rate and a faster rate;
- explicit review of seek/loop/rate-change fallback behavior;
- an affirmative pitch-stability judgement for the reviewed material;
- no critical audible artifact observed in the reviewed cases;
- a runtime-error/log review;
- no device names, track names, track paths or source audio embedded in the JSON.

This is only the listening portion of M2. It does **not** close M2 by itself and does not replace device CPU/callback-deadline/underrun, latency, controller or broader music-domain analysis evidence.

## Build the exact research executable

From a Windows 11 x64 checkout:

```powershell
cmake --preset windows -DBROKEDJ_BUILD_TIMESTRETCH_PROTOTYPE=ON
cmake --build --preset windows-release --parallel 2
ctest --preset windows-release
```

The executable is:

```text
build/windows/BrokeDJ_artefacts/Release/BrokeDJ.exe
```

When using a CI development artifact from this branch, the same witness script and guide are staged beside `BrokeDJ.exe` as `M2-KEYLOCK-LISTENING-WITNESS.ps1` and `M2-KEYLOCK-LISTENING-WITNESS.md`. The package manifest and `SHA256SUMS.txt` cover them together with the exact executable. That staging is convenience and integrity evidence only; CI is still forbidden from minting a listening witness.

Keep hardware volume conservative. The witness script never launches BrokeDJ or produces audio.

## Manual A/B procedure

Use the **same exact executable** for both passes.

### 1. Normal baseline

Launch without the research flag:

```powershell
.\BrokeDJ.exe
```

For at least three representative, user-owned/licensed tracks:

1. confirm ordinary 1.0x playback;
2. review a slower deck rate near the current UI minimum (approximately 0.8x);
3. review a faster deck rate near the current UI maximum (approximately 1.2x);
4. note the expected ordinary pitch change so you have a baseline.

Do not put track names or paths into the evidence file.

### 2. Key-lock research pass

Close the app, then launch the same executable with:

```powershell
.\BrokeDJ.exe --key-lock-research
```

For the same material:

1. while the deck is paused, set a slower rate, then start playback and judge whether pitch remains acceptably stable;
2. pause, set a faster rate, then start playback and repeat;
3. listen for obvious warble, transient smearing, clicks, metallic tearing, channel instability or other critical artifacts;
4. while playing, exercise a seek, loop change and rate change. The current conservative lifecycle intentionally disarms research audio and falls back to normal playback on these live discontinuities;
5. pause after a discontinuity and resume so the message-thread lifecycle can restage the research path;
6. review the BrokeDJ runtime log for unexpected key-lock errors before attesting the session.

The current implementation deliberately prefers reliable fallback over seamless live key-lock automation. A successful witness must not be described as proof of seamless switching or universal material quality.

## Record the witness

From the repository root, after the listening session:

```powershell
pwsh -NoProfile -File .\scripts\m2_keylock_listening_witness.ps1 `
  -AppPath .\build\windows\BrokeDJ_artefacts\Release\BrokeDJ.exe `
  -RepresentativeTrackCount 3 `
  -UserOwnedOrLicensed `
  -NormalBaselineReviewed `
  -SlowKeyLockReviewed `
  -FastKeyLockReviewed `
  -TransportFallbackReviewed `
  -PitchStabilityAcceptable `
  -NoCriticalArtifactsObserved `
  -RuntimeErrorReview
```

For a staged development artifact, run the copy beside the executable instead:

```powershell
pwsh -NoProfile -File .\M2-KEYLOCK-LISTENING-WITNESS.ps1 `
  -AppPath .\BrokeDJ.exe `
  -RepresentativeTrackCount 3 `
  -UserOwnedOrLicensed `
  -NormalBaselineReviewed -SlowKeyLockReviewed -FastKeyLockReviewed `
  -TransportFallbackReviewed -PitchStabilityAcceptable `
  -NoCriticalArtifactsObserved -RuntimeErrorReview
```

The script refuses evidence generation in CI. Common truthy `CI`/`GITHUB_ACTIONS` values (`1`, `true`, `yes`, `on`, case-insensitive) are rejected **before `AppPath` is resolved or hashed**. `-ValidateExisting` remains allowed in CI because it validates an already-created record instead of minting a listening claim.

The output defaults to `BrokeDJ-M2-keylock-listening.json` and contains only the app fingerprint, coarse Windows build/architecture data, the representative-track count and closed boolean attestations. It does not inspect or hash the test music.

Publication is fail-closed. A new run is first serialized to a sibling temporary candidate and fully validated against the exact executable. Only a complete candidate replaces the requested evidence file. If any required count/check is missing or false, the script exits `2`, deletes the temporary candidate and leaves any previously accepted evidence unchanged.

## Validate an existing witness

```powershell
pwsh -NoProfile -File .\scripts\m2_keylock_listening_witness.ps1 `
  -AppPath .\build\windows\BrokeDJ_artefacts\Release\BrokeDJ.exe `
  -EvidencePath .\BrokeDJ-M2-keylock-listening.json `
  -ValidateExisting
```

Validation rejects:

- the wrong executable hash/version/name;
- non-Windows-11-class or non-x64 environment fields;
- fewer than three representative tracks;
- an unlicensed-material attestation;
- string-spoofed booleans/integers;
- incomplete listening checks;
- privacy flags indicating names, paths or source music;
- unexpected/free-form JSON properties.

Exit codes are `0` for a complete matching witness, `2` for an incomplete generation attempt that was not published, and `1` for malformed/mismatched evidence, unsupported host/input, missing files or CI-generation refusal.

Review the JSON before sharing it. The schema intentionally has no free-form notes field so track names, paths and hardware details are not accidentally copied into public CI or issue logs.

## Evidence handling

Keep the JSON with the exact development artifact or test record. Do not commit user music. Do not claim M2 complete from this JSON alone. Production key lock still needs the remaining documented M2 qualification, including device-specific performance/latency evidence and broader representative-material review.
