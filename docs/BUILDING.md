# Building BrokeDJ

## Windows

Use Windows 11 x64, Git, CMake >= 3.24, MSVC from Visual Studio 2022 Build Tools with Desktop development with C++ and a Windows SDK. Open the repository in VS Code with the recommended extensions or use a Developer PowerShell.

```powershell
cmake --preset windows
cmake --build --preset windows-release
ctest --preset windows-release
cmake --install build/windows --config Release --prefix dist/BrokeDJ
```

The executable is `build/windows/BrokeDJ_artefacts/Release/BrokeDJ.exe`. The MSVC runtime is linked statically in this standalone build. `dist/BrokeDJ` is a development staging directory, not a tested installer.

## Linux developer builds

Windows is the first delivery target. Linux can be used for core tests and developer validation. On Debian/Ubuntu, native GUI prerequisites commonly include `build-essential cmake ninja-build git pkg-config libasound2-dev libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev libxcomposite-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev`. Web browser and curl modules are disabled.

```sh
cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux --parallel 2
ctest --test-dir build/linux --output-on-failure
```

## Core-only and sanitizers

```sh
cmake -S . -B build/core -DBROKEDJ_BUILD_APP=OFF -DBROKEDJ_SANITIZE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/core
ctest --test-dir build/core --output-on-failure
```

The sanitizer switch applies to GCC/Clang; MSVC uses the ordinary core tests here. Thread stress plus ASan/UBSan does not replace a ThreadSanitizer run or hardware validation.

## Local BPM / key corpus validator

Normal app builds also compile the developer-only `brokedj_analysis_validator` target. It is not installed with BrokeDJ and does not require an audio device. Use it with a local manifest and music you are allowed to test:

```powershell
.\build\windows\Release\brokedj_analysis_validator.exe .\local-corpus\manifest.tsv
```

The tool runs the current offline BPM/key detector path without using the persistent analysis cache, reports stable manifest IDs rather than source paths and exits non-zero when a supplied reference fails. The repository intentionally does not bundle a music corpus. See [`ANALYSIS_VALIDATION.md`](ANALYSIS_VALIDATION.md) for the manifest contract and evidence rules.

## Opt-in time-stretch/key-lock prototype

The M2 research adapter is deliberately disabled by default so ordinary BrokeDJ playback remains independent of the experimental dependency. To build its deterministic ratio/pitch/seek, owner-lifecycle and realtime-contract tests:

```sh
cmake -S . -B build/timestretch -DBROKEDJ_BUILD_APP=OFF -DBROKEDJ_BUILD_TIMESTRETCH_PROTOTYPE=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/timestretch --parallel 2
ctest --test-dir build/timestretch --output-on-failure -R 'time_stretch|deck_playback_selector|engine_keylock_source|keylock_deck_lifecycle'
```

This fetches Signalsmith Stretch at commit `57b93f4e9206a089a45387eaa39bdc9f310d3308` and Signalsmith Linear at commit `5668673560146a9cfe38c25315071e3fd68c8317`. Passing these tests does not make key lock a production-qualified deck feature; see [`TIME_STRETCH_PROTOTYPE.md`](TIME_STRETCH_PROTOTYPE.md).

### Native developer lifecycle path

When the native application is configured with `BROKEDJ_BUILD_TIMESTRETCH_PROTOTYPE=ON`, the same optional stack can be exercised through the real JUCE application lifecycle without adding a user-facing key-lock button:

```powershell
cmake --preset windows -DBROKEDJ_BUILD_TIMESTRETCH_PROTOTYPE=ON
cmake --build --preset windows-release --parallel 2
ctest --preset windows-release
.\build\windows\BrokeDJ_artefacts\Release\BrokeDJ.exe --key-lock-research
```

`--key-lock-research` is a developer-only opt-in. The app configures and removes the four deck owners only at stopped-audio device boundaries, associates a successfully submitted immutable clip with its owner, and performs non-audio staging from the message-thread lifecycle. A live seek, loop or rate change disarms the optional path immediately so `Engine` falls back to its existing production rate converter; key lock is restaged only after the deck is paused. This conservative behavior is intentional until seamless live restaging, reviewed music-domain listening and physical device deadline/underrun evidence are qualified.

The flag is unavailable in builds where `BROKEDJ_BUILD_TIMESTRETCH_PROTOTYPE=OFF`; ordinary playback remains independent of Signalsmith. It is not a release feature or a claim of live-performance readiness.

## Reproducible dependency / offline configuration

JUCE 9.0.2 is pinned to `72782788ce18c2d4d760b28e0921d6ffc6431102`. CMake fetches it automatically. To prepare offline, clone the upstream repository on a connected machine, check out that exact commit and transfer the entire checkout. Pass `-DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE` when configuring. CMake/MSVC/SDK are build prerequisites and are not installed by the app.

For an offline build of the optional time-stretch prototype, also prepare the exact Signalsmith Stretch and Signalsmith Linear commits listed above and pass `-DFETCHCONTENT_SOURCE_DIR_SIGNALSMITH_STRETCH=/path/to/signalsmith-stretch` and `-DFETCHCONTENT_SOURCE_DIR_SIGNALSMITH_LINEAR=/path/to/linear`. The normal build does not need these two source trees while `BROKEDJ_BUILD_TIMESTRETCH_PROTOTYPE=OFF`.

## Smoke mode

`BrokeDJ --smoke-test` opens the native window with audio-device initialization disabled and exits automatically. This checks the GUI lifecycle only. It does not test playback, sound quality, audio drivers, headphones or latency.

## Silent audio-device capability probe

`BrokeDJ --device-probe` scans the JUCE audio backends and output-device descriptors visible to the current Windows machine and writes **both** `BrokeDJ-device-probe.txt` and schema-versioned `BrokeDJ-device-probe.json` into the current working directory. The probe does not call `AudioIODevice::open`, start an audio callback or emit audio. Descriptor construction is used only in the full local mode so JUCE can expose advertised channel/rate/buffer capabilities.

```powershell
cd .\build\windows\BrokeDJ_artefacts\Release
.\BrokeDJ.exe --device-probe
Get-Content .\BrokeDJ-device-probe.txt
Get-Content .\BrokeDJ-device-probe.json -Raw | ConvertFrom-Json | Format-List
```

The JSON report has an explicit `schema_version` and safety fields including `calls_device_open`, `starts_audio_callback`, `creates_device_descriptors`, `unexpected_open_state_count` and `safety_invariants_ok`. The full probe checks `AudioIODevice::isOpen()` before and after its capability queries. If a descriptor is unexpectedly observed open, BrokeDJ writes the evidence and exits non-zero instead of silently certifying the probe as non-opening.

Treat `four_output_candidate=true` / `four_output_candidate=yes` only as a capability candidate. It does not prove that master 1/2 and private cue 3/4 are physically isolated, that device switching is reliable, or that latency/xrun/listening gates pass. Those M1 checks still require a user-controlled manual session on the real interface. Review both local reports before sharing them because hardware device names may identify your setup.

`--device-probe-ci` is stricter: it enumerates backend/output names without constructing per-device descriptors, then emits the same schema with `creates_device_descriptors=false`. Windows CI parses the JSON, checks schema/safety invariants, verifies that the backend count matches the serialized array and rejects any CI report containing descriptor entries. This validates the diagnostic contract only; it is not hardware qualification.

## Packaging and staged-artifact contract

After the Windows build/tests pass, CI stages `dist/BrokeDJ`, creates `BrokeDJ-source.zip` from the exact workflow commit and writes a privacy-neutral `SOURCE-COMMIT.txt`. The full **development artifact** also keeps validation/audio/GUI/device-probe evidence, witness tooling, source, the package verifier and third-party license material.

`scripts/package_contract.py` creates deterministic development metadata at the artifact root:

- `PACKAGE-MANIFEST.json` — schema-versioned source/version identity plus the size and SHA-256 of every staged development-artifact payload file;
- `SHA256SUMS.txt` — the same sorted payload hashes in a simple reviewable form;
- `BrokeDJ-Beta-Preview-Windows-x64.zip` plus `.sha256` — a separately verified **runtime-subset portable** containing only the staged `BrokeDJ/` tree under one versioned root.

The portable ZIP deliberately excludes the outer `BrokeDJ-source.zip`, `PACKAGE-MANIFEST.json`, `SHA256SUMS.txt` and `VERIFY-PACKAGE.py`. Those development/source-verification files remain available in the full development artifact instead of being duplicated into the consumer archive. The portable still includes the app, notices/licenses and the packaged M1–M4/Beta qualification guides/scripts that are part of the current Beta Preview workflow.

The development manifest intentionally excludes timestamps, machine names and local paths. Creation fails if required runtime/source/license/evidence files are missing, if `SOURCE-COMMIT.txt` does not match the workflow commit, or if a symbolic link enters the payload. Portable creation is bound byte-for-byte to the staged `BrokeDJ/` runtime subset and uses fixed ZIP metadata. Verification rejects missing, extra, resized, hash-mismatched, traversal, symlink or non-deterministic members.

The first Windows job uploads the complete development artifact, including `VERIFY-PACKAGE.py`. A second Windows job downloads that exact artifact into a fresh workspace, verifies the full manifest, extracts the runtime-subset portable into a path containing spaces/Unicode, verifies the extracted tree against the staged runtime before and after no-audio smoke, and launches its `BrokeDJ.exe` with `--smoke-test` and `--device-probe-ci`. Only the portable ZIP and its SHA-256 sidecar are then uploaded under the `BrokeDJ-Beta-Preview-Windows-x64` artifact name. This still does **not** certify a consumer clean machine, HiDPI appearance, real device switching, master/cue hardware isolation, controller support, latency, listening quality or live reliability.

For a downloaded **development artifact**, the optional deep integrity check remains:

```powershell
python .\VERIFY-PACKAGE.py verify --root . --expected-commit <full-40-character-commit-sha>
Get-Content .\SHA256SUMS.txt
```

For the smoke-qualified **Beta Preview** artifact, verify the portable ZIP with Windows PowerShell only:

```powershell
$line = (Get-Content .\BrokeDJ-Beta-Preview-Windows-x64.zip.sha256 -Raw).Trim()
$expected = ($line -split '\s+')[0].ToLowerInvariant()
$actual = (Get-FileHash .\BrokeDJ-Beta-Preview-Windows-x64.zip -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actual -ne $expected) { throw 'BrokeDJ Beta Preview SHA-256 mismatch' }
```

Python is required only for the optional full development-artifact verifier; BrokeDJ itself and the portable checksum check do not require Python. The manual Windows 11/hardware witness remains documented in [`M1_HARDWARE_WITNESS.md`](M1_HARDWARE_WITNESS.md).

`cpack --config build/windows/CPackConfig.cmake -C Release` can still create a local development ZIP. A public alpha/beta/stable package remains blocked until the stronger release gate is satisfied, including exact-head CI, packaged/manual Windows qualification, real audio/multi-output cue evidence where required, functional regression, applicable controller/soak checks, source/notices/checksums and documented known issues. No updater or code-signing credentials are configured.
