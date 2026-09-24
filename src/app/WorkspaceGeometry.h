// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <algorithm>
#include <array>

namespace broke::ui {
// Presentation geometry only; independent of JUCE and the audio engine so the
// complete supported size range can be checked without opening an audio device.
struct Rect final {
    int x = 0, y = 0, width = 0, height = 0;
    [[nodiscard]] constexpr int right() const noexcept { return x + width; }
    [[nodiscard]] constexpr int bottom() const noexcept { return y + height; }
    [[nodiscard]] constexpr bool empty() const noexcept { return width <= 0 || height <= 0; }
    [[nodiscard]] constexpr bool contains(Rect other) const noexcept {
        return !other.empty() && other.x >= x && other.y >= y
            && other.right() <= right() && other.bottom() <= bottom();
    }
    [[nodiscard]] constexpr bool intersects(Rect other) const noexcept {
        return !empty() && !other.empty() && x < other.right() && other.x < right()
            && y < other.bottom() && other.y < bottom();
    }
};

struct WorkspaceGeometry final {
    Rect title, subtitle, library, session, history, audioSettings;
    Rect record, limiter, microphone, booth, boothLevel, microphoneIo;
    Rect status, author, mixer;
    std::array<Rect, 4> decks{}; // A, B, C, D; channel order is A/C | B/D.
};

[[nodiscard]] inline WorkspaceGeometry workspaceGeometry(int width, int height) noexcept {
    WorkspaceGeometry out;
    // These are content sizes, not window sizes. Allow native title-bar/border
    // deductions below the advertised 1050x800 outer-window minimum.
    width = std::max(1000, width);
    height = std::max(720, height);
    constexpr int margin = 16, gap = 6, row = 32;
    out.title = {margin, 12, 172, 38};
    out.subtitle = {margin, 53, 172, 32};
    out.library = {204, 16, 98, row};
    out.session = {out.library.right() + gap, 16, 98, row};
    out.history = {out.session.right() + gap, 16, 98, row};
    out.audioSettings = {width - margin - 174, 16, 174, row};

    int right = width - margin;
    const auto take = [&right](int size) {
        const Rect result{right - size, 56, size, row};
        right -= size + gap;
        return result;
    };
    out.record = take(108);
    out.limiter = take(68);
    out.microphone = take(58);
    out.booth = take(72);
    out.boothLevel = take(144);
    out.microphoneIo = take(82);
    out.author = {width - margin - 90, height - 28, 90, 22};
    out.status = {margin, height - 28, width - margin * 2 - 100, 22};

    constexpr int workTop = 102, sideGap = 10, deckGap = 10;
    const int workHeight = height - workTop - 38;
    const int mixerWidth = std::clamp(width / 4, 288, 360);
    const int sideWidth = (width - margin * 2 - mixerWidth - sideGap * 2) / 2;
    out.mixer = {margin + sideWidth + sideGap, workTop, mixerWidth, workHeight};
    const int topHeight = (workHeight - deckGap) / 2;
    out.decks[0] = {margin, workTop, sideWidth, topHeight};
    out.decks[2] = {margin, workTop + topHeight + deckGap, sideWidth,
                    workHeight - topHeight - deckGap};
    out.decks[1] = {out.mixer.right() + sideGap, workTop,
                    width - margin - out.mixer.right() - sideGap, topHeight};
    out.decks[3] = {out.decks[1].x, out.decks[2].y, out.decks[1].width, out.decks[2].height};
    return out;
}
} // namespace broke::ui
