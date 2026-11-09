// SPDX-License-Identifier: MIT
#include "native_ui.hpp"
#include <cassert>
#include <limits>

int main() {
    using namespace arsenal::ui;
    static_assert(weapon_cell(0, 0).weapon == 2);
    static_assert(weapon_cell(1, 1).weapon == 1);
    static_assert(weapon_cell(2, 4).weapon == 13);
    static_assert(weapon_cell(-1, 0).weapon == -1);
    static_assert(weapon_cell(3, 1).weapon == -1);
    static_assert(kWeaponGrid.size() == 3);
    for (int weapon = 0; weapon < 16; ++weapon) {
        bool found = weapon == 0;  // Wrench belongs to the header.
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 5; ++col)
                if (weapon_cell(row, col).weapon == weapon) found = true;
        assert(found);
    }
    assert(level_color(1) == 0x8FA0D8FFu);
    assert(level_color(5) == 0xFFC830FFu);
    assert(level_color(8) == 0xFFFFFFFFu);
    assert(level_color(9) == 0xFF9030FFu);
    assert(hud_bar_y(true) == 51);
    assert(hud_bar_y(false) == 43);
    assert(ammo_box_width(99) == 70);
    assert(ammo_box_width(100) == 95);
    assert(popup_ticks(true) == 120);
    assert(popup_ticks(false) == 100);
    assert(popup_alpha(0, true) == 0);
    assert(popup_alpha(120, true) == 0);
    assert(popup_alpha(60, true) == 255);
    assert(popup_alpha(1, false) < 20);
    assert(with_alpha(0xFF00AAFFu, 0) == 0xFF00AA00u);
    assert(with_alpha(0xFF00AA00u, 500) == 0xFF00AAFFu);
    assert(filled_width(28.0f, 50) == 14.0f);
    // RAC2-like ammo HUD alignment: x follows the original game slot.
    assert(ammo_xp_x(42) == 47.0f);
    assert(ammo_xp_x(92) == 97.0f);
    assert(ammo_xp_y(false) == 43.0f);
    assert(ammo_xp_y(true) == 51.0f);
    assert(ammo_xp_width(40) == 68.0f);
    assert(ammo_xp_width(100) == 90.0f);
    assert(kAmmoXpHeight == 4.0f);
    assert(kHudElementCount == 13);
    assert(kHudElementStride == 0x90);
    assert(kHudAmmoFadeOffset == 0x70);
    assert(kHudAmmoPointerOffset == 0x80);
    assert(hud_alpha(-1) == 0);
    assert(hud_alpha(0) == 0);
    assert(hud_alpha(4) == 127);
    assert(hud_alpha(8) == 255);
    assert(hud_alpha(12) == 255);
    assert(progress_tween(0, 100) == 5);
    assert(progress_tween(88, 90) == 90);
    assert(progress_tween(100, 90) == 95);
    assert(progress_tween(100, -1) == 95);
    assert(ammo_xp_color(2) == 0xE65B49FFu);
    assert(ammo_xp_color(5) == level_color(5));
    return 0;
}
