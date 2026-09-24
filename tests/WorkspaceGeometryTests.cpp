// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#include "app/WorkspaceGeometry.h"
#include <array>
#include <cstdlib>
#include <iostream>

using broke::ui::Rect;
static void check(bool result, const char* label, int width, int height) {
    if (!result) {
        std::cerr << label << " at " << width << 'x' << height << '\n';
        std::exit(1);
    }
}
int main() {
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
    std::cout << "Workspace geometry: " << cases
              << " sizes passed; main toolbar, four decks and central mixer remain visible.\n";
}
