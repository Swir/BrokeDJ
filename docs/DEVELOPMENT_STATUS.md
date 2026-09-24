# BrokeDJ development status

This file is the durable engineering checkpoint for the current repository state. It records implemented scope, evidence boundaries and the next largest step without treating documentation work as product progress.

## Current delivery target

**First usable Windows 11 x64 Beta.** Scope remains frozen around completing and qualifying M1–M4. M5+ work stays later unless it directly removes a blocker for this target.

## Active packaged qualification checkpoint — PR #110

- Active branch: `feat/package-beta-witness-runner`; draft PR **#110** targets `main`. Code/docs head before this status-only checkpoint is `a48bf790bcabbfc969a1cf9cb0d7eac6aa093bc1`; this checkpoint advances the branch, so only exact final-head CI may authorize merge.
- Audit found a real Beta-preview packaging gap: the repository had a guided `beta_witness_runner.ps1`, but the smoke-qualified portable ZIP did not actually stage that runner or its guide. A tester therefore had the individual M1–M4 tools but not the integrated flow documented as the preferred first-Beta path.
- PR #110 now stages `BETA-WITNESS-RUNNER.ps1`, its guide and `START-BETA-QUALIFICATION.cmd` beside the exact packaged `BrokeDJ.exe`. The launcher uses ordinary Windows PowerShell without elevation or execution-policy changes, binds to the adjacent executable/runner names and keeps accepted evidence in `%USERPROFILE%\Documents\BrokeDJ-Beta-Evidence`.
- Package-smoke was extended so the downloaded staged tree must contain the runner/guide/launcher, the **real extracted portable ZIP** must pass the runner's prompt-free tool-resolution self-test, and the launcher must reference the packaged runner and executable names before the existing relocated no-audio GUI/device-probe smoke and post-smoke byte verification continue.
- The safety boundary is unchanged: CI cannot mint human M1–M4 evidence; the launcher does not auto-start ordinary playback, microphone input or recording; public Beta/Release still requires the documented release gate.
- Pre-status exact-head workflows for `a48bf790bcabbfc969a1cf9cb0d7eac6aa093bc1` started as Build and test **#557** (`35992166496`), M1 **#81** (`35992166507`), M2 **#76** (`35992166599`), M3 **#71** (`35992166498`), Beta qualification **#49** (`35992166514`), Native library scale **#227** (`35992166587`) and Beta witness runner **#33** (`35992166551`). They were still running when this status checkpoint was written and may be superseded/cancelled by the final-head reruns.
- This blocker-removal package does not change `docs/progress.json`: roadmap remains **1/10 (10.0%, PRE-ALPHA)**. M1–M4 still require their genuine Windows 11 hardware/listening/library evidence.

## Merged M3 metering checkpoint — PR #109

- `main` includes merged PR **#109** as `1ab1545e9d88cbce98ae7b32971f6282b7ab1228`; exact PR head was `c1cf176aeccb10ba06ff5b46ed16a884388e5388`.
- Added JUCE-independent sampled channel-meter ballistics for the already-visible first-Beta mixer strips: immediate attack, bounded release, a 1.2 s peak hold with bounded decay, finite/non-finite sanitisation and a manually clearable overload latch. This is message-thread presentation state only; `Engine::process()` is unchanged.
- The four pre-fader strips render a -60..+6 dBFS segmented range, a peak-hold marker and a visible latched overload cap. Left-click clears the UI latch. Tooltips retain the explicit boundary that these are sampled pre-fader meters, not true-peak or loudness measurements.
- The dependency-free `workspace_geometry` CTest also exercises linear-to-dB conversion, release, peak hold/decay, overload latch/reset and NaN/Inf safety.
- Main push workflows after the merge, including Build and test #556 and Beta witness runner #32, completed successfully. This hardening did not close M3 or change the roadmap count.

## Merged runtime-only portable checkpoint — PR #108

- `main` includes squash-merged PR **#108** as `0c7ac000c74e822ac33d94b8bb26fcd870f0cb4d` (`Publish Beta Preview as runtime-only portable bundle`). The exact qualified PR head was `1b873a5d651cefb6fb69eabd7115f60e64e2f077`; its base was the merged runtime-staging cleanup from PR #107 at `5d43d5b2bf380789a6d974782ca5991478f623ac`.
- The complete deterministic development artifact contract remains unchanged at the outer staging root, while the consumer Beta Preview publishes only the portable ZIP plus its SHA-256 sidecar. Development-only `BrokeDJ-source.zip`, `PACKAGE-MANIFEST.json`, `SHA256SUMS.txt` and `VERIFY-PACKAGE.py` are rejected if they leak into the portable root.
- `verify-extracted-portable` binds an already extracted portable byte-for-byte to the exact staged `BrokeDJ/` runtime subset and rejects missing, extra, tampered or symbolic-link payloads. CI runs this verifier before and after relocated no-audio smoke, then publishes only the verified consumer bundle.
- Exact-head qualification for `1b873a5d651cefb6fb69eabd7115f60e64e2f077` passed **Build and test #551**, **Native library scale smoke #221**, **M1 hardware witness tool #79**, **M2 key-lock listening witness tool #74**, **M3 mixer/recording witness tool #69**, **Beta qualification tool #48** and **Beta witness runner tool #27**.
- Build #551 produced a `BrokeDJ-Beta-Preview-Windows-x64` artifact containing only `BrokeDJ-Beta-Preview-Windows-x64.zip` plus its SHA-256 sidecar. Independent post-CI inspection confirmed a matching sidecar, one x86-64 Windows GUI executable, exact `SOURCE-COMMIT.txt` identity, no forbidden development-root files and no JUCE SDK install-tree leak.
- This packaging checkpoint did not satisfy any physical M1–M4 hardware/listening witness. GitHub Releases remains empty; the artifact is a smoke-qualified **Beta Preview development package**, not a public Beta/Release or live-performance qualification.

## Merged deck/theme checkpoint — PR #106

- `main` includes squash-merged PR **#106** as `acf7ecc099348870701f821cc6550c491f44c7f4` (`Add circular decks and selectable first-Beta themes`). The exact qualified PR head was `72b5ca011543dac6120c61d6e5aefa0cda87b59f`.
- Every deck exposes the existing bounded Jog/Scratch transport through a compact circular platter beside its waveform while preserving the existing `JogScratchController` ownership, fail-closed Slip/Beat Loop rules and release-to-restore transport semantics.
- Three persisted runtime accent themes are integrated for the first-Beta workstation: **Electric Blue**, **Ultraviolet** and **Ember**. Theme persistence uses JUCE `PropertiesFile` on the message thread and does not touch realtime audio, transport or session state.
- Exact-head qualification passed Build and test #546, UI visual witness #64, Native library scale smoke #216, M1 #77, M2 #72, M3 #67 and Beta witness runner #22. The final UI witness artifact was manually reviewed for all three themes and compact 1050x800 containment.

## Merged first-Beta workstation baseline

- `main` includes squash-merged PR #105 at `8e5ee002bf9a1c847e76d1fdc15bb6cb01af2bae` (`Make the first-Beta workstation panel usable at every supported size`). The exact code head qualified before merge was `b62b4935e488b6816423e21ce2ff33e8307d7749`.
- PR #105 passed Build and test #532 including native Windows/full CTest and staged + extracted portable-package smoke; UI visual witness #52; Native library scale smoke #202; M1 #70; M2 #65; M3 #60; and Beta witness runner #8.
- The workstation keeps the real four-channel central mixer and channel faders visible throughout the supported size range, adds independent per-deck MIX/GRID views, larger performance waveforms, sampled pre-fader peak strips, one-click Library plus separate Session controls, and explicit non-owning UI handles instead of child-order/caption discovery.
- Native no-audio smoke switches all four decks independently between GRID and MIX at every deterministic resize step and verifies visible-control containment/non-overlap plus preservation of transport/session settings. JUCE-independent `workspace_geometry` covers 53,186 content sizes.

## Verified implementation state

- **M1:** native Windows x64 build/test, staged-package no-audio lifecycle/resize smoke, silent device-capability probing, privacy-safe witness tooling, portable Beta Preview packaging, native four-deck async import/replacement isolation, deterministic device-reprepare transport continuity, conservative loss pause/no-auto-resume behavior, `AudioIODevice::isOpen()` loss detection and fast open-device A-to-B replacement fail-safe are implemented. The remaining gate is real Windows 11 clean-machine switching/loss plus physical four-output master/cue isolation.
- **M2:** reviewed-grid performance controls, continuous Sync, variable-tempo grids, Hot Cues, Beat Jump, REV/SLIP, bounded JOG/SCRATCH and the opt-in Signalsmith-backed key-lock research lifecycle are implemented. Native `MainComponent`/DeckPanel integration covers PLAY/rate/LOOP/CUE 0, live seek and fail-closed clip replacement. Representative real-music BPM/key review, key-lock listening/latency qualification and physical controller workflows remain open.
- **M3:** channel trim, sampled ballistic meters, crossfader laws, master/cue/booth routing, dropout-aware 24-bit WAV set recording, optional microphone ducking, sample-peak limiter measurements and deterministic EQ/master-path tests are implemented. Physical/listening mic, Booth, limiter, recording and long-session gates remain open.
- **M4:** SQLite migrations, bounded search/tags/playlists/history, duplicate/missing/relocate workflows, source-bound waveform/analysis cache, four-deck session persistence, fail-safe library backup/restore, native 5k-track/12k-history staged-EXE recovery, native async session/adoption coverage and privacy-safe two-pass fixture-state verification are integrated. The actual human Windows 11 connected-library/session run remains required.
- **Packaging:** the consumer portable is a deterministic runtime-subset artifact. PR #110 proposes to include the integrated witness runner/guide and local launcher in that already-verified runtime tree so the preferred manual qualification path actually travels with the candidate.
- **Qualification UX:** the guided runner coordinates the canonical M1–M4 human witnesses around one exact candidate executable and deliberately cannot fabricate hardware/listening evidence in CI. PR #110 makes that existing runner directly available inside the portable candidate instead of requiring a matching source checkout.

## Unmet gates

- **PR #110:** newest exact-head Build/package-smoke plus witness-tool workflows must pass. Do not merge from a predecessor head merely because earlier checks were green.
- **M1:** real Windows 11 clean-machine launch/resize/import/playback, live device replacement/loss behavior and physical 4-output master/cue isolation.
- **M2:** representative real-music BPM/key review, key-lock listening, real-device CPU/deadline/underrun/latency evidence and physical controller/wider scratch qualification. Native synthetic integration tests are not substitutes for those human/device gates.
- **M3:** real EQ/mixer listening plus physical microphone/ducking, Booth, recording/dropout and long-session qualification.
- **M4:** perform and review the user-controlled Windows 11 connected library/session witness against an exact candidate executable. Exact-process ownership and schema-2 evidence remove identity ambiguity but do not fabricate launch/resize/import/search/tag/playlist/history/duplicate/missing/relocate/backup/restore/session evidence.
- **Release:** no Alpha/Beta/Stable Release until the documented gate for that scope is satisfied. The current downloadable package is a development **Beta Preview** for hands-on testing, not a physically qualified public Beta/Release.

## Next largest step

Finish exact-head qualification of PR #110 and repair any packaging/tool-resolution regression it exposes. Do **not** widen M5+ scope while the first-Beta gate is externally constrained. Once the smoke-qualified portable contains and validates the guided runner, exercise that exact candidate on real Windows 11 hardware to collect genuine M1–M4 clean-machine/audio/listening/library evidence. Fix any real panel, import, playback, device, recording or session regression exposed by that run before public Beta publication.