# M1 packaged launcher acceptance

The packaged Windows candidate satisfies this M1 launcher slice only when all of these are true:

- `START-BETA-QUALIFICATION.cmd` is present at the package root.
- `M1-HARDWARE-WITNESS.ps1` and `BrokeDJ.exe` are adjacent to it.
- the packaged launcher accepts `M1` mode and binds that mode to the adjacent M1 witness and exact candidate executable.
- `PACKAGE-MANIFEST.json` lists the packaged launcher and witness files.
- the portable ZIP contains the same launcher, witness, and candidate executable.
- the package checksum/provenance still matches the exact PR head.

The source-checkout helper `scripts/start_m1_hardware_witness.cmd` is a convenience path and is not required to be duplicated into the consumer package now that the already-packaged launcher exposes M1-only mode.

Physical device switching, device-loss recovery, four-output master/cue isolation, and listening remain real manual evidence and are not closed by this launcher work.

## Exact-head verification

The previous PR #116 head `b15532ac1c748a050eb28bf59d02c15b13b68e23` passed Build and test #585, Native library scale smoke #255, and Beta witness runner #61. Build #585 artifacts proved that the existing packaged launcher, witness, executable, manifest, and portable ZIP were otherwise valid, while the separate `START-M1-HARDWARE-WITNESS.cmd` was absent.

The current branch replaces that packaging dependency by adding an M1-only mode to the launcher that is already staged and packaged. A newer exact-head build/artifact must still prove that the packaged launcher contains the M1 mode before this acceptance slice can be considered complete.
