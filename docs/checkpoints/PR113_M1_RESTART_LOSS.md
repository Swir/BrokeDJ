# PR #113 — M1 restart/loss qualification checkpoint

Durable checkpoint for the first-Beta M1 gate hardening. This file records tooling state only; it does not claim that physical Windows 11 qualification has passed and it does not change `docs/progress.json`.

- Branch: `feat/m1-restart-loss-witness`
- Pull request: #113 (`Harden M1 restart and device-loss qualification`), draft while exact-head CI is incomplete.
- Base: `main` at `c5f4c044ecd74578e70618734a35a095c3effa15`.
- Product/tooling checkpoint before this status commit: `69edfdd68155e0380dfa96e860d101cc052c038d`.
- Scope: M1 evidence is schema 2 and now requires `deviceLossFailsSafe` plus `deviceSettingsPersistAcrossRestart`; legacy schema-1 M1 evidence is rejected by the M1 validator and by composed Beta qualification.
- Human safety boundary: the witness never starts playback, disconnects hardware, changes device settings, restarts BrokeDJ or changes hardware volume. The tester performs and observes those actions explicitly.
- Contract coverage: the M1 workflow accepts a complete schema-2 fixture and rejects missing loss/restart checks, legacy schema 1, type spoofing, privacy leaks, identity mismatch, unsafe probes and zero-four-output probes. Beta composition also rejects legacy M1 evidence.
- CI started for checkpoint `69edfdd68155e0380dfa96e860d101cc052c038d`: Build and test #570 (`36019580722`), Native library scale smoke #240 (`36019580811`), M1 hardware witness tool #87 (`36019580796`), Beta qualification tool #53 (`36019580759`) and Beta witness runner tool #46 (`36019580791`) were in progress when this checkpoint was written.
- Roadmap remains 1/10 (10.0%, PRE-ALPHA). M1 still requires a real Windows 11 x64 run proving clean launch/resize/import, device switch and reversible loss behavior, output-setting persistence across restart, and physical master 1/2 versus private cue 3/4 isolation.

## Merge gate

Do not merge PR #113 until the exact final PR head passes all required checks. Even a fully green PR does not close M1: the packaged `START-BETA-QUALIFICATION.cmd` still needs genuine user-controlled Windows 11 hardware evidence for the exact candidate.

## Next largest step

Finish exact-head CI for PR #113. If green, merge the qualification hardening; then use the resulting smoke-qualified portable on real Windows 11 hardware and fix any actual device/persistence/routing regression before public Beta publication.
