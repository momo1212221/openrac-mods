// SPDX-License-Identifier: MIT
#pragma once

#include "ui_style.hpp"
#include <array>
#include <algorithm>
#include <cstdint>

namespace arsenal::ui {

// RAC-RS's bars.luau: the native Weapons-page 5x3 icon layout in 512x448 space.
// Values are indices in progression.cpp's weapon_defs(), not item IDs.
constexpr std::array<std::array<int, 5>, 3> kWeaponGrid{{
    {{2, 7, 6, 11, 8}},
    {{5, 1, 3, 9, 4}},
    {{14, 15, 10, 12, 13}},
}};
constexpr float kGridX = 44.8f;
constexpr float kGridY = 110.8f;
constexpr float kGridStep = 42.5f;
constexpr float kMenuBarOffset = 19.5f;
constexpr int kMenuBarWidth = 28;
constexpr int kMenuBarHeight = 3;

struct WeaponCell {
    int weapon;
    float center_x;
    float center_y;
};
constexpr WeaponCell weapon_cell(int row, int col) {
    return row >= 0 && row < 3 && col >= 0 && col < 5
               ? WeaponCell{kWeaponGrid[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)],
                            kGridX + kGridStep * col, kGridY + kGridStep * row}
               : WeaponCell{-1, 0, 0};
}

// The original palette: V1..V5, Mega, Giga, Omega, Endless.
constexpr std::array<std::uint32_t, 8> kLevelColors{{
    0x8FA0D8FFu, 0x5CD66CFFu, 0x4CC3F0FFu, 0xC070F0FFu,
    0xFFC830FFu, 0xFF5CD8FFu, 0x40E8FFFFu, 0xFFFFFFFFu
}};
constexpr std::uint32_t level_color(int level) {
    return level >= 1 && level <= 8 ? kLevelColors[static_cast<std::size_t>(level - 1)]
                                   : 0xFF9030FFu;
}
constexpr int ammo_box_width(int max_ammo) { return max_ammo >= 100 ? 95 : 70; }
constexpr int hud_top(bool ntsc) { return ntsc ? 18 : 10; }
constexpr int hud_bar_y(bool ntsc) { return hud_top(ntsc) + 32 + 1; }

// Going Commando-style weapon XP: a slim red strip belonging to the game's
// existing ammo badge. The original HUD element supplies the actual x and fade,
// so we never render a detached panel at a fixed screen position.
constexpr int kHudElementCount = 13;
constexpr int kHudElementStride = 0x90;
constexpr int kHudAmmoValueOffset = 0x08;
constexpr int kHudAmmoXOffset = 0x50;
constexpr int kHudAmmoFadeOffset = 0x70;
constexpr int kHudAmmoPointerOffset = 0x80;
constexpr int kHudOpenFadeSteps = 8;
constexpr float kAmmoXpXOffset = 5.0f;
constexpr float kAmmoXpHeight = 4.0f;

constexpr float ammo_xp_width(int max_ammo) { return max_ammo >= 100 ? 90.0f : 68.0f; }
constexpr float ammo_xp_x(int native_ammo_x) { return static_cast<float>(native_ammo_x) + kAmmoXpXOffset; }
constexpr float ammo_xp_y(bool ntsc) { return static_cast<float>(hud_bar_y(ntsc)); }
constexpr int hud_alpha(int fade_count) {
    return std::clamp(fade_count, 0, kHudOpenFadeSteps) * 255 / kHudOpenFadeSteps;
}
constexpr int progress_tween(int displayed, int target) {
    const int difference = std::clamp(target, 0, 100) - std::clamp(displayed, 0, 100);
    return std::clamp(displayed, 0, 100) + std::clamp(difference, -5, 5);
}
// The first game has no weapon XP HUD, so use the red upgrade line of RAC2,
// tinting completed and high-level weapons in their level colours.
constexpr std::uint32_t ammo_xp_color(int level) {
    return level < 5 ? 0xE65B49FFu : level_color(level);
}

// A 2-second HUD level-up, fading in and out for 0.4 seconds on either disc.
constexpr int ticks_per_second(bool ntsc) { return ntsc ? 60 : 50; }
constexpr int popup_ticks(bool ntsc) { return ticks_per_second(ntsc) * 2; }
constexpr int popup_fade_ticks(bool ntsc) { return ticks_per_second(ntsc) * 2 / 5; }
constexpr std::uint8_t popup_alpha(int remaining, bool ntsc) {
    const int total = popup_ticks(ntsc);
    const int fade = popup_fade_ticks(ntsc);
    const int clamped = remaining < 0 ? 0 : (remaining > total ? total : remaining);
    const int age = total - clamped;
    const int ramp = std::min(std::min(age, clamped), fade);
    return static_cast<std::uint8_t>((ramp * 255) / fade);
}
constexpr std::uint32_t with_alpha(std::uint32_t rgba, int alpha) {
    return (rgba & 0xFFFFFF00u) | static_cast<std::uint32_t>(std::clamp(alpha, 0, 255));
}
}  // namespace arsenal::ui
