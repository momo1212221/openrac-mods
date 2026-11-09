// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cmath>

namespace arsenal::ui {
constexpr int kWeaponsPerPage = 8;
constexpr int kNanotechRow = kWeaponsPerPage;
constexpr int kPageSwitchRow = kWeaponsPerPage + 1;

constexpr int weapon_for_row(int page, int row) {
    return (page >= 0 && page <= 1 && row >= 0 && row < kWeaponsPerPage)
               ? page * kWeaponsPerPage + row
               : -1;
}

inline int percent(double fraction) {
    if (!std::isfinite(fraction)) return 0;
    return std::clamp(static_cast<int>(std::lround(fraction * 100.0)), 0, 100);
}

inline float filled_width(float width, int pct) {
    if (!std::isfinite(width) || width <= 0.0f) return 0.0f;
    return width * static_cast<float>(std::clamp(pct, 0, 100)) / 100.0f;
}
}  // namespace arsenal::ui
