// SPDX-License-Identifier: MIT
#pragma once

#include "ui_style.hpp"
#include <array>
#include <algorithm>
#include <cstdint>

namespace arsenal::ui {

// The actual RAC1 Weapons menu contains only the weapons the player owns.
// It does NOT have a fixed 5x3 grid: a new save may show just 1x1 or 1x3.
// Reproduce the game's menu_grid_item_grid geometry instead of guessing
// screen coordinates or requiring ten equipped weapons.
constexpr int kMaxMenuCells = 48;

struct MenuCellPlacement {
    float x;
    float y;
    float width;
    float height;
};

struct MenuGridGeometry {
    bool valid = false;
    float first_x = 0.0f;
    float first_y = 0.0f;
    float step_x = 0.0f;
    float step_y = 0.0f;
    float icon_width = 0.0f;
    float icon_height = 0.0f;

    MenuCellPlacement cell(int row, int col) const {
        return MenuCellPlacement{first_x + step_x * col, first_y + step_y * row,
                                 icon_width, icon_height};
    }
};

inline MenuGridGeometry menu_grid_geometry(
    int rows, int cols, int flags, int grid_width, int grid_height,
    float panel_width, float panel_height, float margin_x, float margin_y,
    float icon_width, float icon_height
) {
    if (rows <= 0 || cols <= 0 || rows * cols > kMaxMenuCells
        || grid_width <= 0 || grid_height <= 0
        || panel_width <= 0.0f || panel_height <= 0.0f
        || icon_width <= 0.0f || icon_height <= 0.0f) return {};
    const float panel_extent = std::max(panel_width, panel_height);
    const float scale = static_cast<float>(std::max(grid_width, grid_height)) / panel_extent;
    const float start_x = cols >= 2 ? margin_x : (panel_width - icon_width) * 0.5f;
    const float step_x = cols >= 2
        ? icon_width + (panel_width - 2.0f * margin_x - icon_width * cols) / (cols - 1)
        : 0.0f;
    const float start_y = (flags & 2) || rows >= 2
        ? margin_y : (panel_height - icon_height) * 0.5f;
    const float step_y = (flags & 2) ? icon_height + 0.15f
        : (rows >= 2
               ? icon_height + (panel_height - 2.0f * margin_y - icon_height * rows) / (rows - 1)
               : 0.0f);
    return MenuGridGeometry{true, start_x * scale, start_y * scale,
                            step_x * scale, step_y * scale, icon_width * scale,
                            icon_height * scale};
}

constexpr int native_grid_bar_width(float icon_width) {
    return std::clamp(static_cast<int>(icon_width) - 2, 8, 28);
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
constexpr int kHudAmmoPointerOffset = 0x0C;
constexpr int kHudOpenFadeSteps = 8;
constexpr float kAmmoXpXOffset = 85.0f;
constexpr float kAmmoXpHeight = 4.0f;

constexpr float ammo_xp_width(int max_ammo) { return max_ammo >= 100 ? 48.0f : 42.0f; }
constexpr float ammo_xp_x(int native_ammo_x) { return static_cast<float>(native_ammo_x) + kAmmoXpXOffset; }
constexpr float ammo_xp_y(bool ntsc) { return static_cast<float>(hud_bar_y(ntsc) + 12); }
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
