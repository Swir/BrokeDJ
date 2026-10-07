# M1 one-click hardware witness

For the packaged Windows candidate, use the already-packaged launcher:

`START-BETA-QUALIFICATION.cmd M1`

That mode runs only `M1-HARDWARE-WITNESS.ps1` against the adjacent `BrokeDJ.exe` and stores evidence under:

`%USERPROFILE%\Documents\BrokeDJ-M1-Evidence`

The normal no-argument launcher behavior remains the full guided M1–M4 Beta qualification flow.

For a source checkout, build BrokeDJ first, then run the packaged-candidate qualification path above from the staged/portable candidate. The current `scripts/start_m1_hardware_witness.cmd` source helper is not a qualification path because its adjacent-file assumptions do not match the source tree layout; do not use it as evidence until that helper is corrected and covered by CI.

Playback, device switching, device loss/recovery, cue routing, and listening checks remain manual real-hardware qualification. The launcher does not fabricate or auto-approve those observations.
