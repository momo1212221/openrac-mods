// SPDX-License-Identifier: MIT
#include "ui_style.hpp"

#include <cassert>
#include <cmath>
#include <limits>

int main() {
    using namespace arsenal::ui;
    static_assert(weapon_for_row(0, 0) == 0, "first weapon");
    static_assert(weapon_for_row(0, 7) == 7, "end first page");
    static_assert(weapon_for_row(1, 0) == 8, "start second page");
    static_assert(weapon_for_row(1, 7) == 15, "last weapon");
    static_assert(weapon_for_row(1, 8) == -1, "control is not a weapon");
    static_assert(weapon_for_row(-1, 0) == -1, "invalid page");
    static_assert(weapon_for_row(2, 0) == -1, "invalid page");

    assert(percent(0.0) == 0);
    assert(percent(0.523) == 52);
    assert(percent(1.0) == 100);
    assert(percent(1.3) == 100);
    assert(percent(-0.4) == 0);
    assert(percent(std::numeric_limits<double>::infinity()) == 0);
    assert(percent(std::numeric_limits<double>::quiet_NaN()) == 0);
    assert(filled_width(100.0f, 0) == 0.0f);
    assert(filled_width(100.0f, 50) == 50.0f);
    assert(filled_width(100.0f, 100) == 100.0f);
    assert(filled_width(100.0f, 150) == 100.0f);
    assert(filled_width(100.0f, -20) == 0.0f);
    assert(filled_width(-1.0f, 50) == 0.0f);
    return 0;
}
