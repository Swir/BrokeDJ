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

## Reproducible dependency / offline configuration

JUCE 9.0.2 is pinned to `72782788ce18c2d4d760b28e0921d6ffc6431102`. CMake fetches it automatically. To prepare offline, clone the upstream repository on a connected machine, check out that exact commit and transfer the entire checkout. Pass `-DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE` when configuring. CMake/MSVC/SDK are build prerequisites and are not installed by the app.

## Smoke mode

`BrokeDJ --smoke-test` opens the native window with audio-device initialization disabled and exits automatically. This checks the GUI lifecycle only. It does not test playback, sound quality, audio drivers, headphones or latency.

## Packaging

After a successful build, `cpack --config build/windows/CPackConfig.cmake -C Release` creates a development ZIP. Corresponding source, third-party license material and manual release evidence are required before a public production release. No updater or code-signing credentials are configured. No secret or token is needed for ordinary local builds.
