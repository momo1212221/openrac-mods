// SPDX-License-Identifier: MIT
// Copyright (c) 2026 AsobiCodes
// OpenRAC native UI port: Moritz Wallinger
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "openrac/openrac_mod.h"

namespace arsenal::native_ui {

// Coordinates are in OpenRAC's native 512 x 448 picture (API 1.14).
// The overlay is only called during gameplay, never on the pause menu.
// All data is copied from the simulation thread before the renderer reads it.
struct LevelUp {
    int weapon = -1;
    int level = 1;
    std::uint64_t tick = 0;
};
struct Snapshot {
    bool enabled = false;
    bool playing = false;
    bool pal = true;
    int held_weapon = -1;
    bool has_ammo = false;
    bool big_ammo = false;
    int level = 1;
    float progress = 0.0f;
    float smooth_progress = 0.0f;
    std::uint64_t tick = 0;
    std::array<LevelUp, 3> popups{};
    int popup_count = 0;
};

inline std::uint32_t rgba(int r, int g, int b, int a = 255) {
    return (static_cast<std::uint32_t>(r) << 24) |
           (static_cast<std::uint32_t>(g) << 16) |
           (static_cast<std::uint32_t>(b) << 8) |
           static_cast<std::uint32_t>(std::clamp(a, 0, 255));
}
inline std::uint32_t level_colour(int level, int alpha = 255) {
    // RAC-RS bars.luau's V1/V5/Mega/Giga/Omega/Endless palette.
    constexpr std::array<std::uint32_t, 8> colours{
        0x8FA0D8u, 0x5CD66Cu, 0x4CC3F0u, 0xC070F0u,
        0xFFC830u, 0xFF5CD8u, 0x40E8FFu, 0xFFFFFFu
    };
    const std::uint32_t rgb = level >= 9 ? 0xFF9030u :
        colours[static_cast<std::size_t>(std::clamp(level, 1, 8) - 1)];
    return (rgb << 8) | static_cast<std::uint32_t>(std::clamp(alpha, 0, 255));
}

inline void rect(const openrac_overlay& d, float x, float y, float w, float h,
                 std::uint32_t colour) {
    if (d.rect) d.rect(d.context, x, y, w, h, colour);
}
inline void text(const openrac_overlay& d, float x, float y, float height,
                 std::uint32_t colour, const char* value) {
    if (d.text) d.text(d.context, x, y, height, colour, value);
}
inline void progress_bar(const openrac_overlay& d, float x, float y, float width,
                         float height, float progress, std::uint32_t fill, int alpha) {
    const float p = std::clamp(progress, 0.0f, 1.0f);
    rect(d, x - 1, y - 1, width + 2, height + 2, rgba(5, 10, 18, alpha * 3 / 4));
    rect(d, x, y, width, height, rgba(29, 36, 55, alpha * 3 / 4));
    if (p > 0.0f) {
        rect(d, x, y, width * p, height, fill);
        rect(d, x, y, width * p, 1, rgba(255, 255, 255, alpha / 3));
    }
}

inline void draw(const openrac_overlay* overlay, const Snapshot& s) {
    if (overlay == nullptr || !s.enabled || !s.playing) return;
    const auto& d = *overlay;

    // HUD below the game's own ammo panel. Never draw fake activation messages.
    if (s.held_weapon >= 0) {
        const float y = (s.pal ? 10.0f : 18.0f) + 32.0f + 2.0f;
        char version[8]{};
        std::snprintf(version, sizeof version, "V%d", std::clamp(s.level, 1, 99));
        text(d, 12, y - 5, 9, level_colour(s.level), version);
        const float width = s.big_ammo ? 115.0f : 90.0f;
        progress_bar(d, 36, y, width, 4, s.smooth_progress, level_colour(s.level), 224);
    }

    // Game-like, language-independent level-up cards: V number only, fade in and out.
    constexpr std::uint64_t kLifeTicks = 120; // nominal 2 seconds at 60 Hz
    constexpr std::uint64_t kFadeTicks = 18;
    int drawn = 0;
    for (int i = 0; i < s.popup_count && i < static_cast<int>(s.popups.size()); ++i) {
        const auto& p = s.popups[static_cast<std::size_t>(i)];
        if (p.weapon < 0 || s.tick < p.tick) continue;
        const auto age = s.tick - p.tick;
        if (age >= kLifeTicks) continue;
        const float fade_in = std::min(1.0f, static_cast<float>(age + 1) / kFadeTicks);
        const float fade_out = std::min(1.0f, static_cast<float>(kLifeTicks - age) / kFadeTicks);
        const int alpha = static_cast<int>(235.0f * std::min(fade_in, fade_out));
        const float y = 59.0f + 32.0f * static_cast<float>(drawn++);
        const float slide = (1.0f - fade_in) * -6.0f;
        rect(d, 218, y + slide, 76, 27, rgba(8, 16, 31, alpha * 3 / 4));
        rect(d, 218, y + slide, 3, 27, level_colour(p.level, alpha));
        char version[8]{};
        std::snprintf(version, sizeof version, "V%d", std::clamp(p.level, 1, 99));
        text(d, 238, y + 5 + slide, 17, level_colour(p.level, alpha), version);
    }
}

} // namespace arsenal::native_ui
