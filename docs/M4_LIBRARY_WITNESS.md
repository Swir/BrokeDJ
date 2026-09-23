# M4 Windows 11 library witness

This witness closes the remaining manual interaction gate for BrokeDJ's M4 library/session milestone. It does **not** qualify physical audio routing, controller support, live performance, M1 hardware, M2 key-lock/listening quality, M3 microphone/booth/recording hardware, or a public release.

## Safety and privacy

The witness prepares its own disposable synthetic WAV workspace, so personal music is not required. Each run creates a **new content identity**: a quiet 48 kHz stereo 16-bit PCM 440 Hz tone carries a tiny random per-run nonce inside the generated samples, and the duplicate is copied byte-for-byte from that primary file. This prevents stale rows from an older witness run from sharing the current fixture SHA-256 while still giving duplicate detection an exact byte-identical pair.

The script itself never starts playback. Keep system/headphone volume conservative if you choose to start the fixture for the History check. The fixture workspace exists only for the current run and is removed when the script exits. Its local paths are printed for operator guidance but are never written into accepted evidence. BrokeDJ must not delete or overwrite source media while exercising duplicate review, missing-file handling, relocation, backup/restore or session persistence.

The accepted witness file stores only Windows build/architecture, BrokeDJ executable filename/version/SHA-256, timestamps and boolean pass/fail results. It deliberately does not ask for track names, track paths, screenshots, account data or source music. The evidence schema remains closed and rejects unexpected fields.

A second, temporary verification report is generated **after** the human workflow and before evidence is accepted. The witness passes the synthetic fixture SHA-256 to the exact BrokeDJ executable through a process-only environment variable, launches `BrokeDJ.exe --m4-fixture-state`, and requires a privacy-safe aggregate result for only that hash. The app serializes counts only: track count, missing-track count, history count, tag-association count and playlist-membership count plus bounded file-presence reconciliation counts. It does not serialize source paths, titles, artists, tag names, playlist names or history timestamps. The temporary report is deleted after validation and is not copied into the accepted witness file.

Evidence generation is rejected before resolving or launching `AppPath` when either `CI` or `GITHUB_ACTIONS` has a common truthy value (`1`, `true`, `yes`, `on`, case-insensitive). `-ValidateExisting` remains usable in CI. `-FixtureSelfTest` creates two independent disposable fixtures, proves that each run gets a different content hash while each primary/duplicate pair remains byte-identical, validates the fixture WAV contract and exercises positive/negative aggregate-report validation. It creates no human qualification evidence.

## Prepare

Use the exact Windows x64 staged executable you want to qualify. Current CI development artifacts place the witness recorder and this guide beside the executable:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\M4-LIBRARY-WITNESS.ps1 -AppPath ".\BrokeDJ.exe"
```

From a source checkout:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\m4_library_witness.ps1 -AppPath "C:\path\to\BrokeDJ.exe"
```

Before asking manual questions, the recorder fails closed unless it can:

1. reject unattended CI generation;
2. confirm Windows 11 build 22000+ on an x64 OS from an x64 PowerShell process;
3. resolve exactly `BrokeDJ.exe` and fingerprint its version plus SHA-256;
4. run that executable with `--smoke-test` in a disposable directory;
5. validate the no-audio GUI/resize report;
6. create and validate the per-run-unique primary WAV, its byte-identical duplicate and relocation directory.

The automatic preflight catches a broken executable, deterministic resize/geometry regression or malformed fixture before the longer workflow. It does **not** replace manual Windows usability/HiDPI review.

## Required checks

1. **Launch and resize** — the app launches on Windows 11 and remains usable through meaningful window-size changes; critical deck, mixer and library controls do not overlap or disappear.
2. **Import and search** — import the generated primary fixture and confirm bounded library search finds it.
3. **Tags and playlists** — edit a tag on the fixture; create a playlist; add/remove/browse membership; refresh and confirm persistence. Finish with at least one tag association and at least one playlist membership still attached to the fixture so the aggregate verifier can prove retention.
4. **History** — start the generated fixture once under normal user control and verify a local History entry appears. The script itself never starts playback.
5. **Duplicate and missing review** — import the byte-identical duplicate and exercise `is:duplicate`. Move the primary fixture into the printed relocation directory, choose **Library → Refresh file status**, wait for the bounded background refresh summary, then exercise `is:missing`. Confirm the moved primary is discovered without first trying to load it, the duplicate/missing workflows remain review-only, and source media is not deleted or overwritten. Any unresolved filesystem probes block the witness.
6. **Relocate** — reconnect the existing primary library record to the moved fixture. Confirm its tag and playlist membership remain attached to the same record.
7. **Library backup/restore** — create a backup, make a harmless metadata change, restore the backup and verify the previous state returns.
8. **Session save/load** — save a four-deck/mixer session, alter controls, reload it and verify expected state is restored. Restored decks must remain paused until explicit Play.

A failed manual check is a real M4 blocker. If any answer is `no`, generation stops before creating or replacing evidence.

## Connected fixture-state verification

After all eight answers pass, the recorder asks you to close BrokeDJ and wait for it to exit so SQLite state is settled. It then launches the **same fingerprinted executable** in the no-audio `--m4-fixture-state` mode. That mode opens only the local BrokeDJ library database and uses the fixture-scoped M4 primitives; no application window or audio device is opened and no callback starts.

The verifier requires all of the following before evidence can be written:

- the bounded fixture-only presence refresh is complete;
- at least the two expected byte-identical fixture rows were scanned;
- zero fixture rows remain missing and zero refresh results are unresolved;
- at least two fixture rows share the current run's exact SHA-256;
- at least one History entry, tag association and playlist membership exist for that fixture content identity.

Because the fixture SHA-256 is unique per run, stale synthetic rows from an earlier run cannot satisfy or poison this check. The targeted refresh cannot update unrelated user tracks. If the app report is malformed, incomplete, reports a different mode, opens audio, returns a non-zero result or fails any aggregate condition, the witness writes no accepted evidence.

When the connected verifier passes, the recorder serializes the normal witness candidate to a temporary file, validates the closed evidence schema plus executable fingerprint, and only then replaces the final evidence path. A failed or interrupted new run therefore does not deliberately overwrite an earlier accepted witness with incomplete data.

## Validate saved evidence

For a staged development artifact:

```powershell
.\M4-LIBRARY-WITNESS.ps1 `
  -AppPath ".\BrokeDJ.exe" `
  -EvidencePath ".\BrokeDJ-M4-Library-Witness.json" `
  -ValidateExisting
```

From a source checkout:

```powershell
.\scripts\m4_library_witness.ps1 `
  -AppPath "C:\path\to\BrokeDJ.exe" `
  -EvidencePath ".\BrokeDJ-M4-Library-Witness.json" `
  -ValidateExisting
```

`-ValidateExisting` validates the exact BrokeDJ M4 field set, schema/scope, Windows 11 x64 environment fields, strict JSON integer/string/boolean types, privacy flags and every required workflow result. It recalculates the selected executable filename, file version and SHA-256 and requires all three to match the saved evidence. The JSON is still an auditable user witness record, not a digital signature or proof that a person performed an action.

## Acceptance boundary

Automated CI covers SQLite migrations, bounded search, tags/playlists/history, content hashing, duplicate/missing directives, paged and fixture-scoped file-presence reconciliation, moved-file rebinding, privacy-safe fixture aggregate counts, waveform/analysis cache ownership, bounded session snapshots and native library backup/restore recovery against synthetic data. Filesystem probes and SQLite maintenance stay outside the audio callback.

The connected verifier strengthens the manual witness by checking real persisted application state for the current synthetic fixture instead of relying only on eight yes/no answers. It still does not establish real audio-device behavior, physical cue isolation, controller timing, listening quality or live reliability.

M4 may be marked complete only after accepted evidence is reviewed against the exact app build and there is no unresolved critical library/session regression. `docs/progress.json` must remain unchanged until that review is real.
