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
    return 0;
}
