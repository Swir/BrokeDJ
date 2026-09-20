# Third-party notices

BrokeDJ-authored code and original artwork: Copyright (c) 2026 Swir, AGPL-3.0-only. Full license: `LICENSE`.

JUCE 9.0.2: Raw Material Software Limited and contributors. Pinned commit: `72782788ce18c2d4d760b28e0921d6ffc6431102` from https://github.com/juce-framework/JUCE . This project selects JUCE's **AGPLv3** option, not an assertion of eligibility for a commercial JUCE plan. Upstream license: https://github.com/juce-framework/JUCE/blob/72782788ce18c2d4d760b28e0921d6ffc6431102/LICENSE.md . Upstream dependencies and notices: the pinned `JUCE.spdx.json` and module source files. Preserve applicable notices when distributing.

Signalsmith Stretch is used only by the opt-in M2 time-stretch/key-lock prototype (`BROKEDJ_BUILD_TIMESTRETCH_PROTOTYPE=ON`), not by ordinary BrokeDJ playback. Pinned upstream commit: `57b93f4e9206a089a45387eaa39bdc9f310d3308` from https://github.com/Signalsmith-Audio/signalsmith-stretch . License: MIT, Copyright (c) 2022 Geraint Luff / Signalsmith Audio Ltd. Upstream license: https://github.com/Signalsmith-Audio/signalsmith-stretch/blob/57b93f4e9206a089a45387eaa39bdc9f310d3308/LICENSE.txt .

Signalsmith Linear is the header-only DSP dependency used by that optional prototype. Upstream release `0.3.1` resolves to pinned commit `5668673560146a9cfe38c25315071e3fd68c8317` from https://github.com/Signalsmith-Audio/linear . License: MIT, Copyright (c) 2025 Signalsmith Audio. Upstream license: https://github.com/Signalsmith-Audio/linear/blob/5668673560146a9cfe38c25315071e3fd68c8317/LICENSE.txt . Preserve both MIT notices with any future distributed binary/source bundle that actually incorporates this prototype.

SQLite 3.53.4: SQLite authors and contributors. BrokeDJ pins the official 2026 amalgamation archive `sqlite-amalgamation-3530400.zip` from https://www.sqlite.org/2026/ with SHA3-256 `628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e`. SQLite source code is dedicated to the public domain; upstream copyright/public-domain statement: https://www.sqlite.org/copyright.html . BrokeDJ compiles the amalgamation into the local library database adapter with extension loading disabled; no external database service or cloud account is required.

GitHub Actions checkout and upload-artifact are build tooling, not runtime application modules; workflow references are pinned to inspected commits.

No proprietary VST3 effect binaries, external ASIO SDK, AI weights, music or third-party sample packs are bundled at this stage. The Signalsmith prototype remains opt-in research infrastructure and is not wired into the application playback path. Future VST3 hosting, proprietary plugin interoperation, ASIO support and AI models need their own license/provenance review before distribution. A roadmap entry is not an integration or license grant.

Corresponding BrokeDJ source: https://github.com/Swir/BrokeDJ . Build instructions and immutable dependency references are included with the project. All upstream trademarks belong to their respective owners; no endorsement is implied.
