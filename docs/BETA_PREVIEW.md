# BrokeDJ Beta Preview

This workflow artifact is a **user-testable Windows 11 x64 Beta Preview**. It is uploaded only after the exact staged package passes the repository's automated build, core/audio tests, package-contract verification, native GUI lifecycle/resize smoke and silent device-probe smoke.

It is **not** a public Beta release, live-performance certification or proof that the manual M1-M4 hardware/listening gates have passed. The roadmap counter remains unchanged until its acceptance criteria are genuinely satisfied.

## Run the preview

1. Extract the complete workflow artifact to a normal writable local folder.
2. Open the `BrokeDJ` folder.
3. Start `BrokeDJ.exe`.
4. Import only music that you own or are allowed to use.
5. Exercise the ordinary four-deck, mixer, library/session and recording workflows before using the packaged witness procedures.

Keep the package together. `SOURCE-COMMIT.txt`, package metadata and the qualification/witness files intentionally travel with the executable so any report can be tied to the exact candidate.

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
