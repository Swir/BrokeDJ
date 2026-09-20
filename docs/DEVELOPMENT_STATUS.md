# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It is not a release-readiness, sound-quality or live-performance claim.

## Current checkpoint

- Default branch baseline: `main` at `fcc52b6ad731c0aafa12ea47468e72214317e9a2`; PR #47 (`M2: add bounded continuous reviewed-grid Sync lock`) is already merged.
- Active development: PR #48 (`M1: verify staged Windows artifact integrity and launch`) from `feat/m1-staged-artifact-contract`.
- First exact-head package run `35516016674` for PR head `c986192a111b616010fd24f820b2ac45b1778268` completed with Linux sanitizers/package-tool tests and the Windows build/test/staging job green, but the separate downloaded-artifact verification job failed before launch.
- The failed uploaded artifact was downloaded and reproduced outside the workflow. Its manifest contained staged JUCE dotfiles that `actions/upload-artifact` had omitted by default, so checksum verification correctly reported those manifested files as missing.
- The same evidence exposed a second source-identity bug: pull-request jobs were packaging GitHub's synthetic merge SHA through `${{ github.sha }}` rather than the actual PR head SHA, despite the package contract claiming exact source identity.
- Repair implementation head `9edfc7621ee843e1cd80abe03624c8de93700742` makes both build jobs explicitly check out the PR head (falling back to `github.sha` for push/dispatch), uses that same SHA throughout package identity checks, and uploads hidden staged files so the uploaded tree matches the manifest.
- This checkpoint commit creates a newer PR head, so merge remains blocked until the workflow for that final exact head is fully green.
- Roadmap source of truth remains `docs/progress.json`: 1/10 equal-weight milestones complete (10.0%, PRE-ALPHA).
- GitHub Releases remains empty; no public BrokeDJ Release exists or is qualified by this checkpoint.

## Active M1 slice: staged artifact integrity and no-build-tree smoke

1. **Deterministic package/source integrity contract**
   - `scripts/package_contract.py` creates a schema-versioned `PACKAGE-MANIFEST.json` and sorted `SHA256SUMS.txt` over the staged Windows development payload.
   - The contract requires the executable, source archive, source-commit witness, repository/JUCE notices and retained CI evidence; it rejects missing/extra/tampered files, path traversal, duplicate entries and staged symbolic links.
   - Manifest content omits timestamps, machine names and local paths so the contract itself does not publish private workstation data.

2. **Exact source and upload identity**
   - Pull-request build jobs explicitly check out `github.event.pull_request.head.sha`; push/workflow-dispatch runs fall back to `github.sha`.
   - Windows staging writes that same source identity to `BrokeDJ/SOURCE-COMMIT.txt`, archives source from the checked-out `HEAD`, copies validation/audio/GUI/device-probe evidence and then creates/verifies the manifest before upload.
   - The uploaded artifact includes hidden files because the staged JUCE payload can legally contain dotfiles that are part of the checksum manifest; omission at the transport layer is therefore treated as an integrity failure, not ignored.

3. **Downloaded staged-artifact launch gate**
   - A separate Windows job downloads the uploaded development artifact into a fresh job workspace, verifies hashes/source identity and launches the staged `BrokeDJ.exe` rather than the build-tree executable.
   - It runs only the no-audio `--smoke-test` and `--device-probe-ci` contracts; it does not open an audio device or emit sound.
   - Passing this gate proves artifact integrity plus no-build-tree lifecycle/probe launch on the Windows runner, not a consumer clean-machine or physical-hardware qualification.

## Gates still open

- PR #48 exact-final-head Linux/Windows/package-smoke CI must pass after the upload/source-identity repair before merge; repair any further regression first.
- M1 still requires real Windows 11 clean-machine/manual resize/HiDPI/import/device-switching checks and real four-output master 1/2 versus cue 3/4 verification.
- Representative user-owned/licensed music-domain BPM/key/grid evidence remains open.
- Production key-lock listening/latency, MIDI/controller mappings and concrete controller profiles remain unqualified.
- Physical storage/underrun behavior, multi-hour soak and public alpha/beta Release qualification remain open.

## Next largest step

First close PR #48 on its exact final head: the downloaded package must verify and both no-audio staged executable checks must pass after the source-identity/upload repair. If that gate is green, integrate only at the normal BrokeDJ cadence. Then continue finish-first work on remaining internally closable M2 analysis/key-lock/controller qualification while preserving the manual M1 hardware gates.
