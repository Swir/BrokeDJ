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

PR #116 head `97e39d0ad76a4ffd15309c46413080daa2076d78` passed Build and test #599, Native library scale smoke #269, Beta witness runner #75, M1 hardware witness tool #102, Beta qualification #62, M2 #95, and M3 #90.

The exact-head development artifact is `11384275124` with GitHub digest `sha256:178499b798b712599068d85d85fcdbd13feac2f2d0081f093a128bb48a562f5d`. The exact-head Beta Preview artifact is `11384305582` with GitHub digest `sha256:61203c7baadc09829e85ef23af28159b99da970f3011d2a86f6341326d8989f7`.

Both artifacts carry the exact `SOURCE-COMMIT.txt`, pass `VERIFY-PACKAGE.py verify` and extracted-portable verification, and contain byte-identical portable ZIP payloads with SHA-256 `5ecd77347a257ec889e14db896ed8e0dbee57ae77fb56f860b05f3359a154b96`. The staged and extracted portable launchers both contain the guarded M1 route and bind it to the adjacent exact packaged candidate. The packaged M1 guide also contains the muted-headphone reprepare check added at this head.

This closes the packaged-launcher acceptance slice for PR #116 at the current exact head. It does not close the M1 roadmap milestone: genuine Windows 11 device switching/loss recovery and physical four-output master/cue isolation still require real hardware evidence.
