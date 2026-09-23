# M4 Windows 11 library witness

This witness closes the remaining manual interaction gate for BrokeDJ's M4 library/session milestone. It does **not** qualify physical audio routing, controller support, live performance, M1 hardware, M2 key-lock/listening quality, M3 microphone/booth/recording hardware, or a public release.

## Safety and privacy

Use disposable copies or non-critical local tracks. BrokeDJ must not delete or overwrite the original music while exercising duplicate review, missing-file handling, relocation, backup/restore or session persistence.

The witness recorder stores only Windows build/architecture, BrokeDJ executable filename/version/SHA-256, timestamps and boolean pass/fail results. It deliberately does not ask for track names, track paths, screenshots, account data or source music. Review any evidence before publishing it. The evidence schema is closed: validation rejects unrecognized top-level, environment, app, check or privacy fields so free-form notes or accidental path/name fields cannot silently become part of an accepted witness record.

Evidence generation is deliberately rejected before resolving or launching `AppPath` when either `CI` or `GITHUB_ACTIONS` has a common truthy value (`1`, `true`, `yes`, `on`, case-insensitive). `-ValidateExisting` remains usable in CI so the schema/fingerprint validator can be regression-tested without fabricating a human witness.

## Prepare

Use the exact Windows x64 development/staged executable you want to qualify. Current CI development artifacts place the witness recorder and this guide beside the staged executable and include both in the package manifest/checksums. From the extracted `BrokeDJ` directory, the shortest path is:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\M4-LIBRARY-WITNESS.ps1 -AppPath ".\BrokeDJ.exe"
```

When working from a source checkout instead, use:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\m4_library_witness.ps1 -AppPath "C:\path\to\BrokeDJ.exe"
```

Before asking any manual questions, the recorder performs a fail-closed preflight:

1. rejects unattended CI evidence generation before touching the supplied executable;
2. confirms Windows 11 build 22000+ on an x64 OS from an x64 PowerShell process;
3. resolves exactly `BrokeDJ.exe` and fingerprints its version plus SHA-256;
4. runs the selected executable with `--smoke-test` in a disposable temporary directory;
5. requires the generated GUI-smoke report to confirm success, no audio playback, no audio-device open and at least one workstation-layout resize step.

The temporary GUI-smoke report is removed after preflight and is not inserted into M4 evidence. This automatic no-audio check catches a broken executable or deterministic resize/geometry regression before the longer manual workflow, but it **does not replace** the first human launch/resize check below: hosted or scripted geometry cannot establish real Windows usability, HiDPI appearance or interaction quality.

The script then records the following user-controlled checks without automating music playback or playing loud audio on its own.

## Required checks

1. **Launch and resize** — the app launches on Windows 11 and remains usable through meaningful window-size changes; critical deck, mixer and library controls do not overlap or disappear.
2. **Import and search** — import a supported local test track and confirm bounded library search can find it.
3. **Tags and playlists** — edit tags; create a playlist; add/remove/browse membership; refresh and confirm persistence.
4. **History** — start the test track once under normal user control and verify a local history entry appears. The script itself never starts playback.
5. **Duplicate and missing review** — exercise `is:duplicate` and `is:missing`; confirm the workflow is review-only and does not delete source music.
6. **Relocate** — move a disposable test copy outside BrokeDJ, mark/review it as missing, then use Relocate. Confirm tags and playlist membership remain attached to the same library record.
7. **Library backup/restore** — create a backup, make a harmless metadata change, restore the backup and verify the previous state returns.
8. **Session save/load** — save a four-deck/mixer session, alter controls, reload it and verify expected state is restored. Restored decks must remain paused until explicit Play.

A failed check is a real M4 blocker. If any answer is `no`, generation stops before creating or replacing the evidence file. Fix the product or repeat the witness after the fix; do not edit a failed result into `true` and treat it as evidence.

When every check passes, the recorder serializes a candidate into a temporary file in the destination directory, validates the complete closed schema plus executable fingerprint, and only then replaces the final evidence path. The candidate is cleaned up in all cases. A failed or interrupted new run therefore does not deliberately overwrite an earlier accepted evidence file with incomplete data.

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

`-ValidateExisting` validates the exact BrokeDJ M4 field set, schema/scope, Windows 11 x64 environment fields, strict JSON integer/string/boolean types, privacy flags and every required workflow result. It also recalculates the selected `AppPath` filename, file version and SHA-256 and requires all three to match the saved evidence. This catches accidental review against the wrong executable and rejects simple type-spoofing such as the string `"true"` being substituted for a real JSON boolean or `"26100"` for an integer Windows build. Unexpected fields are rejected rather than preserved as accepted evidence.

The JSON is still an auditable user witness record, not a digital signature or proof that a person performed an action. It remains editable by whoever controls the file. Treat legitimacy as the combination of the user-controlled workflow, exact executable fingerprint, CI state and review history; manually changing a failed result does not create valid qualification evidence.

## Acceptance boundary

Automated CI already covers SQLite migrations, bounded search, tags/playlists/history, content hashing, duplicate/missing directives, moved-file rebinding, waveform/analysis cache ownership, bounded session snapshots and native library backup/restore recovery against synthetic data. The preflight additionally proves that the exact selected executable passes its no-audio native GUI resize/geometry contract before the human workflow begins. Neither automated layer replaces the connected Windows 11 **user workflow**.

M4 may be marked complete only after the evidence is reviewed against the exact app build and there is no unresolved critical library/session regression. The milestone counter in `docs/progress.json` must remain unchanged until that review is real.
