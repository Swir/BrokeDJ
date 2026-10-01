# M1 packaged launcher acceptance

The Windows package is ready for this part of M1 only when all of these are true:

- `START-M1-HARDWARE-WITNESS.cmd` is present at the package root.
- `M1-HARDWARE-WITNESS.ps1` and `BrokeDJ.exe` are adjacent to it.
- `PACKAGE-MANIFEST.json` lists the launcher.
- the portable ZIP contains the same launcher.
- the launcher still binds the witness to the adjacent BrokeDJ candidate.

Physical device switching, device-loss recovery and master/cue listening remain real manual evidence.
