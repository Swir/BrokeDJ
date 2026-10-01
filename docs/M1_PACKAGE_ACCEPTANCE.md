# M1 packaged launcher acceptance

The Windows package is ready for this part of M1 only when all of these are true:

- `START-M1-HARDWARE-WITNESS.cmd` is present at the package root.
- `M1-HARDWARE-WITNESS.ps1` and `BrokeDJ.exe` are adjacent to it.
- `PACKAGE-MANIFEST.json` lists the launcher.
- the portable ZIP contains the same launcher.
- the launcher still binds the witness to the adjacent BrokeDJ candidate.

Physical device switching, device-loss recovery and master/cue listening remain real manual evidence.
## Exact-head verification

PR #116 head `fdd903d925167213908a241d53bb6b7076a9630b` passed Build and test #584, Native library scale smoke #254, and Beta witness runner #60. Build #584 produced development artifact `11136983392` and Beta Preview artifact `11136828842`. Green CI does not close package acceptance until `START-M1-HARDWARE-WITNESS.cmd` is physically present at package root, listed in `PACKAGE-MANIFEST.json`, and present in the portable ZIP.

