# BrokeDJ Beta Preview

This workflow artifact is a **user-testable Windows 11 x64 Beta Preview candidate**. It is uploaded only after the exact staged package passes the repository's automated build, core/audio tests, package-contract verification, native GUI lifecycle/resize smoke and silent device-probe smoke.

It is **not** a public Beta release, live-performance certification or proof that the manual M1-M4 hardware/listening gates have passed. The roadmap counter remains unchanged until its acceptance criteria are genuinely satisfied.

## Portable package

The workflow artifact contains `BrokeDJ-Beta-Preview-Windows-x64.zip` plus `BrokeDJ-Beta-Preview-Windows-x64.zip.sha256`. The ZIP is generated from the already-manifested staged tree with fixed member ordering/metadata, a single versioned top-level directory and no symlinks.

Package verification first validates the loose staged payload against `PACKAGE-MANIFEST.json` / `SHA256SUMS.txt`, then requires the portable archive to contain the exact same member set **and the exact same bytes**. Every archived member is compared with the staged source by size and SHA-256; fixed timestamp, deflate compression and regular-file metadata are also part of the deterministic archive contract. A coherently re-manifested ZIP with different payload bytes therefore cannot pass merely because its own inner manifest is self-consistent. The outer SHA-256 sidecar, duplicate/unsafe paths, encrypted members, symlinks and bounded uncompressed size are checked before temporary extraction and a second inner-manifest verification.

The Windows package-smoke job also extracts the actual portable ZIP into a relocated path containing spaces and a non-ASCII character, verifies the extracted inner manifest against the exact workflow commit, launches that extracted `BrokeDJ.exe` with the no-audio GUI lifecycle/resize smoke and silent device-probe CI mode, removes generated smoke files and verifies the extracted payload again. This catches packaging/path/runtime failures that a loose staged-tree launch cannot expose.

This remains an integrity, reproducibility, extraction and no-audio runtime contract, not a clean-machine or audio-hardware qualification. Real Windows 11 hardware/listening evidence remains manual.

## Run the preview

1. Keep the `.zip` and `.zip.sha256` together until you have verified or extracted the package.
2. Extract `BrokeDJ-Beta-Preview-Windows-x64.zip` to a normal writable local folder.
3. Open the versioned `BrokeDJ-<version>-Beta-Preview-Windows-x64` folder, then open its `BrokeDJ` folder.
4. Start `BrokeDJ.exe`.
5. Import only music that you own or are allowed to use.
6. Exercise the ordinary four-deck, mixer, library/session and recording workflows before using the packaged witness procedures.

Keep the extracted package together. `SOURCE-COMMIT.txt`, package metadata and the qualification/witness files intentionally travel with the executable so any report can be tied to the exact candidate.

For an optional full package-integrity review, run the verifier **before extraction from the downloaded workflow-artifact root**, where `VERIFY-PACKAGE.py`, the loose staged tree, `PACKAGE-MANIFEST.json`, `SHA256SUMS.txt`, the portable ZIP and its `.sha256` sidecar are still together:

```powershell
python .\VERIFY-PACKAGE.py verify --root . --expected-commit <full-40-character-commit-sha>
Get-Content .\SHA256SUMS.txt
Get-Content .\BrokeDJ-Beta-Preview-Windows-x64.zip.sha256
```

Do not run that full `verify` command from inside the extracted versioned ZIP root: by design the outer portable ZIP and sidecar are not members of themselves, so a full outer-package verification there would be incomplete. CI separately verifies the extracted inner payload before and after its relocated no-audio runtime smoke. Python is required only for the optional artifact-integrity review; BrokeDJ itself does not require Python.

## Test first

- clean launch, resize and import on Windows 11 x64;
- playback on all four decks plus seek/loop/hot-cue/beat-jump and REV/SLIP/JOG;
- master output and, when supported by the interface, isolated CUE 3/4 and Booth 5/6;
- gain/EQ/crossfader, microphone/ducking and set recording;
- save/load session, library search/playlists/tags, backup/restore and moved/missing-file recovery;
- repeated device changes and clean shutdown.

If a test fails, keep this exact package and report the reproduction steps. Do not use the preview as the only playback system at a live event.

## Qualification

The package contains the M1-M4 witness guides plus `BETA-QUALIFICATION.ps1`. Those tools are human-controlled; CI is deliberately unable to mint their manual evidence.

Only after all required M1-M4 evidence validates against this exact `BrokeDJ.exe` can the qualifier create `BrokeDJ-Beta-Qualification.json`. A public Beta still additionally requires the repository release gate, source/notices/checksum integrity, known-issues review and post-publication verification.
