# Musical-analysis validation harness

BrokeDJ includes a developer-only local corpus validator for BPM and musical-key estimates. The repository does **not** redistribute music for this purpose. Use only audio you are allowed to test, and keep the corpus outside the repository unless its redistribution rights are explicit.

The validator is evidence tooling, not a claim that the current detectors are professionally accurate. Synthetic CTest fixtures remain useful regression tests; representative music-domain results must be recorded separately before stronger M2 accuracy claims are made.

## Build

On Windows 11 x64, the validator is built with the normal application configuration:

```powershell
cmake --preset windows
cmake --build --preset windows-release --parallel 2
```

The executable is produced by the `brokedj_analysis_validator` CMake target. It is not installed with the BrokeDJ application and is not part of a public release.

## Manifest

Create a UTF-8 tab-separated file outside the repository. Each non-comment row has exactly six columns:

```text
id<TAB>audio<TAB>bpm|-<TAB>tolerance|-<TAB>tonic(0..11)|-<TAB>major|minor|-
```

Example:

```text
# id    audio                 bpm     tolerance   tonic   mode
track01 audio/track01.wav     128.0   1.0         0       minor
track02 audio/track02.flac    174.0   1.5         -       -
track03 D:\DJTests\song.wav   -       -           9       major
```

`audio` may be absolute or relative to the manifest directory. `tonic` uses C=0, C#/Db=1, …, B=11. A row may validate BPM, key, or both. When BPM is omitted, its tolerance must also be `-`; when key is omitted, both tonic and mode must be `-`.

Use stable IDs that do not contain private path information. Validator output reports those IDs rather than the source paths.

## Run

```powershell
.\build\windows\Release\brokedj_analysis_validator.exe .\local-corpus\manifest.tsv
```

To cap each detector to a different analysis window for a controlled experiment:

```powershell
.\build\windows\Release\brokedj_analysis_validator.exe .\local-corpus\manifest.tsv --max-seconds 180
```

Accepted `--max-seconds` values are 5–1800. The tool bypasses the persistent analysis cache so every validation run exercises the detector implementation against the current source audio.

Per-row output records pass/fail, detected BPM/error/confidence and detected key/confidence. The summary reports reference counts and BPM mean absolute error. The process exits non-zero when a source/reference row fails or the manifest is malformed, making it suitable for a private/local validation job without turning copyrighted audio into a repository dependency.

## Evidence rules

When recording results in BrokeDJ documentation, include the validator commit, corpus size, music categories, reference provenance, audio formats/sample rates, configured analysis duration and aggregate metrics. Do not publish private paths or redistribute the source tracks unless their license explicitly permits it.

A green local corpus run does not replace manual beat-grid review, key review, hardware playback tests or listening tests. A corpus should include constant-tempo and genuine variable-tempo material, multiple genres, sparse/dense mixes and challenging rhythmic/tonal cases rather than only tracks known to suit the current detector.
