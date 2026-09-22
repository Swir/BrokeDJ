# Windows UI visual qualification

BrokeDJ keeps a dedicated no-audio Windows pixel witness for the current Beta UI. This evidence is a regression aid, not a substitute for human Windows 11 HiDPI/usability review.

## What CI captures

The `UI visual witness` workflow builds the native Windows x64 application with the same JUCE/CMake path used by the development build, launches `BrokeDJ.exe --smoke-test`, and captures both the compact 1050x800-class state and a workstation-class state. Smoke mode disables audio-device initialization and exits automatically after the deterministic resize sequence.

The capture manifest records the observed window sizes, output filenames, image dimensions, payload sizes, the `PrintWindow` mode used, and explicit `plays_audio=false` / `opens_audio_device=false` safety fields. The workflow rejects missing images, invalid manifest fields, an absent compact/workstation class, or a resize sequence that did not request both 1050x800 and 1600x900.

## Deterministic pixel sanity gate

`scripts/validate_ui_witness.ps1` samples both committed-run PNG artifacts and writes `BrokeDJ-ui-quality.json`. It intentionally checks only properties that can be measured repeatably without pretending to score aesthetics:

- mean luminance remains inside a broad dark-workstation envelope;
- luminance variation is large enough to reject blank or unpainted captures;
- a bounded amount of bright detail remains present;
- the BrokeDJ blue/cyan accent family remains materially present;
- quantized colour diversity stays above a minimum needed to reject corrupt/flat images;
- enough non-black content is painted to reject an empty frame.

The thresholds are deliberately broad. They are not a design score and must not be tightened to make a screenshot look "better" numerically. A future palette redesign should update the validator in the same reviewed UI package, with new witness images inspected before merge.

Hosted Windows runners can expose off-screen black pixels when a requested workstation window is wider than the available desktop. The validator therefore does not require every captured pixel to be painted. The witness manifest records whether the workstation image is the requested class or the largest observed fallback.

## What still requires a person

Before integrating a UI change, inspect both current PNGs and confirm that critical deck, mixer, transport, library/navigation and master controls remain readable and do not overlap or clip at the captured sizes. Real Windows 11 review is still required for HiDPI scaling, keyboard/focus navigation, language fit, manual interaction quality and visual density.

The pixel gate does not certify accessibility, audio routing, latency, controller behavior, listening quality, physical hardware, clean-machine installation or live readiness. Passing it only means the captured no-audio native UI is structurally present and still resembles the intended BrokeDJ visual family closely enough for a human review to be meaningful.
