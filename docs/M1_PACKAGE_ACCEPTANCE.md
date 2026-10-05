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

PR #116 head `77c34f34dbdee99d23b682de9043f8416f56b8d2` passed Build and test #597, Native library scale smoke #267, Beta witness runner #73, M1 hardware witness tool #100, Beta qualification #60, M2 #93, and M3 #88.

The exact-head development artifact is `11365449948` with GitHub digest `sha256:06e26d29b22e81987940564aa8255a0eb31666730a5e9740bd1d342f746c64f6`. The exact-head Beta Preview artifact is `11366358217` with GitHub digest `sha256:8e2462cdd27b95eb29a99e53e7c0d1304a4f2968a96ea0f77d83c0b467fd4424`.

Both artifacts carry the exact `SOURCE-COMMIT.txt`, pass `VERIFY-PACKAGE.py verify` and extracted-portable verification, and contain byte-identical portable ZIP payloads with SHA-256 `c958d1f26de5475877a3c811d5f73a4dbd7d1f35111507993c752696dbf66273`. The staged and extracted portable launchers both contain the guarded M1 route and bind it to the adjacent exact packaged candidate.

This closes the packaged-launcher acceptance slice for PR #116. It does not close the M1 roadmap milestone: genuine Windows 11 device switching/loss recovery and physical four-output master/cue isolation still require real hardware evidence.
