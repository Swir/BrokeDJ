# M4 Windows 11 library witness

This witness closes the remaining manual interaction gate for BrokeDJ's M4 library/session milestone. It does **not** qualify physical audio routing, controller support, live performance, M1 hardware, M2 key-lock/listening quality, M3 microphone/booth/recording hardware, or a public release.

## Safety and privacy

Use disposable copies or non-critical local tracks. BrokeDJ must not delete or overwrite the original music while exercising duplicate review, missing-file handling, relocation, backup/restore or session persistence.

The witness recorder stores only Windows build/architecture, BrokeDJ executable filename/version/SHA-256, timestamps and boolean pass/fail results. It deliberately does not ask for track names, track paths, screenshots, account data or source music. Review any evidence before publishing it.

## Prepare

Use the exact Windows x64 development/staged executable you want to qualify. From PowerShell:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\m4_library_witness.ps1 -AppPath "C:\path\to\BrokeDJ.exe"
```

The script first verifies Windows 11 and fingerprints the exact EXE. It then records the following user-controlled checks without automating or playing loud audio on its own.

## Required checks

1. **Launch and resize** — the app launches on Windows 11 and remains usable through meaningful window-size changes; critical deck, mixer and library controls do not overlap or disappear.
2. **Import and search** — import a supported local test track and confirm bounded library search can find it.
3. **Tags and playlists** — edit tags; create a playlist; add/remove/browse membership; refresh and confirm persistence.
4. **History** — start the test track once under normal user control and verify a local history entry appears. The script itself never starts playback.
5. **Duplicate and missing review** — exercise `is:duplicate` and `is:missing`; confirm the workflow is review-only and does not delete source music.
6. **Relocate** — move a disposable test copy outside BrokeDJ, mark/review it as missing, then use Relocate. Confirm tags and playlist membership remain attached to the same library record.
7. **Library backup/restore** — create a backup, make a harmless metadata change, restore the backup and verify the previous state returns.
8. **Session save/load** — save a four-deck/mixer session, alter controls, reload it and verify expected state is restored. Restored decks must remain paused until explicit Play.

A failed check is a real M4 blocker. Fix the product or repeat the witness after the fix; do not edit the JSON from `false` to `true` manually and treat it as evidence.

## Validate saved evidence

```powershell
.\scripts\m4_library_witness.ps1 `
  -AppPath "C:\path\to\BrokeDJ.exe" `
  -EvidencePath ".\BrokeDJ-M4-Library-Witness.json" `
  -ValidateExisting
```

`-ValidateExisting` rejects unsupported evidence schemas and any incomplete required check. The `AppPath` parameter remains mandatory so the command shape stays explicit, although validation reads the fingerprint from the saved evidence rather than re-running the UI witness.

## Acceptance boundary

Automated CI already covers SQLite migrations, bounded search, tags/playlists/history, content hashing, duplicate/missing directives, moved-file rebinding, waveform/analysis cache ownership, bounded session snapshots and native library backup/restore recovery against synthetic data. This manual witness exists to prove the connected Windows 11 **user workflow** rather than re-count those automated checks.

M4 may be marked complete only after the evidence is reviewed against the exact app build and there is no unresolved critical library/session regression. The milestone counter in `docs/progress.json` must remain unchanged until that review is real.
