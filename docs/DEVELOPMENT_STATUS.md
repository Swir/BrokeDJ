# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `fcc52b6ad731c0aafa12ea47468e72214317e9a2`; PR #47 (`M2: add bounded continuous reviewed-grid Sync lock`) is already merged.
- Active development: PR #48 (`M1: verify staged Windows artifact integrity and launch`) from `feat/m1-staged-artifact-contract`.
- Implementation head before this checkpoint-only documentation commit: `0c19fa77b1c694860bf02cd326822597136deb3a`.
- Build and test run `35515981121` was queued for that implementation head when this checkpoint was recorded. The checkpoint commit itself creates a newer PR head, so merge remains blocked until the workflow for the final exact head is fully green.
- Local deterministic package-contract self-test passed for the committed script logic: create -> verify -> deliberate payload tamper -> expected verification failure. This is not a Windows/package runtime result.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA).
- GitHub Releases remains empty; no public BrokeDJ Release exists or is qualified by this checkpoint.

## Active M1 slice: staged artifact integrity and no-build-tree smoke

1. **Deterministic package/source integrity contract**
   - `scripts/package_contract.py` creates a schema-versioned `PACKAGE-MANIFEST.json` and sorted `SHA256SUMS.txt` over the staged Windows development payload.
   - The contract requires the executable, source archive, source-commit witness, repository/JUCE notices and retained CI evidence; it rejects missing/extra/tampered files, path traversal, duplicate entries and staged symbolic links.
   - Manifest content omits timestamps, machine names and local paths so the contract itself does not publish private workstation data.

2. **Exact source and evidence carried with the artifact**
   - Windows staging writes `BrokeDJ/SOURCE-COMMIT.txt`, archives source from the exact workflow `HEAD`, copies validation/audio/GUI/device-probe evidence and then creates/verifies the manifest before upload.
   - The verifier is copied into the development artifact as `VERIFY-PACKAGE.py`; Python is optional verification tooling and is not a BrokeDJ runtime dependency.

3. **Downloaded staged-artifact launch gate**
   - A separate Windows job downloads the uploaded development artifact into a fresh job workspace, verifies hashes/source identity and launches the staged `BrokeDJ.exe` rather than the build-tree executable.
   - It runs only the no-audio `--smoke-test` and `--device-probe-ci` contracts; it does not open an audio device or emit sound.
   - Passing this gate will prove artifact integrity plus no-build-tree lifecycle/probe launch on the Windows runner, not a consumer clean-machine or physical-hardware qualification.

## Gates still open

- PR #48 exact-final-head Linux/Windows/package-smoke CI must pass before merge; repair any regression first.
- M1 still requires real Windows 11 clean-machine/manual resize/HiDPI/import/device-switching checks and real four-output master 1/2 versus cue 3/4 verification.
- Representative user-owned/licensed music-domain BPM/key/grid evidence remains open.
- Production key-lock listening/latency, MIDI/controller mappings and concrete controller profiles remain unqualified.
- Physical storage/underrun behavior, multi-hour soak and public alpha/beta Release qualification remain open.

## Next largest step

First close or diagnose PR #48 on its final exact head. If its full Linux + Windows + downloaded-package gate is green, integrate only at the normal BrokeDJ cadence. Then continue finish-first work on the remaining internally closable M2 analysis/key-lock/controller qualification while preserving the manual M1 hardware gates.
