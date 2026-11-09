// SPDX-License-Identifier: MIT
#include "native_ui.hpp"
#include <cassert>
#include <limits>

int main() {
    using namespace arsenal::ui;
    // The original weapons page can be 1x1, 1x3, 3x5 or larger.
    const auto single = menu_grid_geometry(1, 1, 0, 160, 96, 160.0f, 96.0f,
                                            2.0f, 2.0f, 24.0f, 24.0f);
    assert(single.valid);
    assert(single.cell(0, 0).x == 68.0f);
    assert(single.cell(0, 0).y == 36.0f);
    const auto few = menu_grid_geometry(1, 3, 0, 160, 96, 160.0f, 96.0f,
                                         8.0f, 4.0f, 24.0f, 24.0f);
    assert(few.valid);
    assert(few.cell(0, 0).x == 8.0f);
    assert(few.cell(0, 1).x == 68.0f);
    assert(few.cell(0, 2).x == 128.0f);
    assert(few.cell(0, 2).y == few.cell(0, 0).y);
    const auto many = menu_grid_geometry(3, 5, 0, 160, 96, 160.0f, 96.0f,
                                          8.0f, 4.0f, 24.0f, 24.0f);
    assert(many.valid);
    assert(many.cell(2, 4).x == 128.0f);
    assert(many.cell(0, 0).y == 4.0f);
    assert(many.cell(2, 0).y == 68.0f);
    const auto fixed_rows = menu_grid_geometry(2, 1, 2, 160, 96, 160.0f, 96.0f,
                                                8.0f, 4.0f, 24.0f, 24.0f);
    assert(fixed_rows.valid);
    assert(fixed_rows.cell(1, 0).y > fixed_rows.cell(0, 0).y);
    assert(!menu_grid_geometry(0, 5, 0, 160, 96, 160.0f, 96.0f,
                               8.0f, 4.0f, 24.0f, 24.0f).valid);
    assert(!menu_grid_geometry(50, 5, 0, 160, 96, 160.0f, 96.0f,
                               8.0f, 4.0f, 24.0f, 24.0f).valid);
    assert(native_grid_bar_width(23.0f) == 21);
    assert(native_grid_bar_width(4.0f) == 8);
    assert(native_grid_bar_width(100.0f) == 28);
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
    assert(ammo_xp_x(42) == 127.0f);
    assert(ammo_xp_x(92) == 177.0f);
    assert(ammo_xp_y(false) == 55.0f);
    assert(ammo_xp_y(true) == 63.0f);
    assert(ammo_xp_width(40) == 42.0f);
    assert(ammo_xp_width(100) == 48.0f);
    assert(kAmmoXpHeight == 4.0f);
    assert(kHudElementCount == 13);
    assert(kHudElementStride == 0x90);
    assert(kHudAmmoFadeOffset == 0x70);
    assert(kHudAmmoPointerOffset == 0x0C);
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
