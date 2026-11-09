// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <algorithm>

namespace arsenal_mastery {

inline constexpr std::array<double, 4> kBaseXpThresholds = {{750.0, 2250.0, 5000.0, 10000.0}};
inline constexpr std::array<double, 5> kPowerMultipliers = {{1.0, 1.2, 1.5, 1.9, 2.5}};

constexpr double xp_threshold(double xp_scale, int next_level) {
    return next_level >= 2 && next_level <= 5
               ? kBaseXpThresholds[static_cast<std::size_t>(next_level - 2)]
                     * std::max(0.0, xp_scale)
               : 0.0;
}

constexpr int weapon_level(double xp, double xp_scale) {
    int level = 1;
    for (int next = 2; next <= 5; ++next) {
        if (xp >= xp_threshold(xp_scale, next)) {
            level = next;
        } else {
            break;
        }
    }
    return level;
}

constexpr double power_multiplier(int level) {
    return kPowerMultipliers[static_cast<std::size_t>(std::clamp(level, 1, 5) - 1)];
}

}  // namespace arsenal_mastery
