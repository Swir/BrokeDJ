// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#include "app/WorkspaceGeometry.h"
#include "core/MeterBallistics.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using broke::ui::Rect;
static void check(bool result, const char* label, int width, int height) {
    if (!result) {
        std::cerr << label << " at " << width << 'x' << height << '\n';
        std::exit(1);
    }
}
static void checkMeter(bool result, const char* label) {
    if (!result) {
        std::cerr << "Meter ballistics: " << label << '\n';
        std::exit(1);
    }
}
int main() {
    {
        broke::MeterBallistics meter;
        auto state = meter.snapshot();
        checkMeter(state.displayDb == broke::MeterBallistics::floorDb
                       && state.holdDb == broke::MeterBallistics::floorDb
                       && !state.overloadLatched,
                   "reset state is finite silence with no overload latch");

        meter.pushLinear(0.5f, false, 0.04f);
        state = meter.snapshot();
        checkMeter(state.displayDb > -6.1f && state.displayDb < -5.9f,
                   "linear peak converts to the expected sampled dBFS range");
        checkMeter(std::abs(state.holdDb - state.displayDb) < 0.01f,
                   "new peak updates the hold marker immediately");

        const float firstPeak = state.displayDb;
        meter.pushLinear(0.0f, false, 0.04f);
        state = meter.snapshot();
        checkMeter(state.displayDb < firstPeak && state.displayDb > firstPeak - 1.0f,
                   "display releases gradually instead of dropping to silence");
        checkMeter(std::abs(state.holdDb - firstPeak) < 0.01f,
                   "peak hold survives the first release sample");

        for (int tick = 0; tick < 31; ++tick) meter.pushLinear(0.0f, false, 0.04f);
        state = meter.snapshot();
        checkMeter(state.holdDb < firstPeak,
                   "peak hold decays after the bounded hold interval");
        checkMeter(state.holdDb >= state.displayDb,
                   "decaying hold marker never falls below the displayed meter");

        meter.reset();
        meter.pushLinear(1.0f, false, 0.04f);
        state = meter.snapshot();
        checkMeter(state.overloadLatched,
                   "a sampled zero-dBFS peak latches overload even without a separate flag");
        meter.pushLinear(0.01f, false, 0.04f);
        checkMeter(meter.snapshot().overloadLatched,
                   "overload remains latched across later quiet samples");
        meter.clearOverloadLatch();
        checkMeter(!meter.snapshot().overloadLatched,
                   "operator reset clears only the overload latch");

        meter.reset();
        meter.pushLinear(std::numeric_limits<float>::quiet_NaN(), false,
                         std::numeric_limits<float>::infinity());
        state = meter.snapshot();
        checkMeter(std::isfinite(state.displayDb) && std::isfinite(state.holdDb)
                       && state.displayDb == broke::MeterBallistics::floorDb,
                   "non-finite input and timer deltas fail safe to finite silence");
        meter.pushLinear(0.25f, true, 1.0f);
        state = meter.snapshot();
        checkMeter(state.overloadLatched && std::isfinite(state.displayDb),
                   "explicit engine overload latches and a stalled UI timer stays bounded");
    }

    std::size_t cases = 0;
    for (int width = 1000; width <= 3840; width += 7) {
        for (int height = 720; height <= 2160; height += 11) {
            const Rect root{0, 0, width, height};
            const auto ui = broke::ui::workspaceGeometry(width, height);
            const std::array<Rect, 19> controls{ui.title, ui.subtitle, ui.library, ui.session,
                ui.history, ui.audioSettings, ui.record, ui.limiter, ui.microphone,
                ui.booth, ui.boothLevel, ui.microphoneIo, ui.status, ui.author,
                ui.mixer, ui.decks[0], ui.decks[1], ui.decks[2], ui.decks[3]};
            for (std::size_t a = 0; a < controls.size(); ++a) {
                check(root.contains(controls[a]), "Containment", width, height);
                for (std::size_t b = a + 1; b < controls.size(); ++b)
                    check(!controls[a].intersects(controls[b]), "Overlap", width, height);
            }
            for (auto deck : ui.decks) {
                check(deck.width >= 328 && deck.height >= 285, "Deck usability floor", width, height);
            }
            check(ui.mixer.width >= 288, "Four real mixer strips", width, height);
            check(ui.decks[0].x == ui.decks[2].x && ui.decks[1].x == ui.decks[3].x,
                  "Stable A/C left and B/D right", width, height);
            ++cases;
        }
    }
    std::cout << "Meter ballistics: finite attack/release/hold/latch checks passed.\n";
    std::cout << "Workspace geometry: " << cases
              << " sizes passed; main toolbar, four decks and central mixer remain visible.\n";
}