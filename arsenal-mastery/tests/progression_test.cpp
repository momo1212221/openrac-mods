// SPDX-License-Identifier: MIT
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "progression.h"

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void expect_near(double actual, double expected, const char* message) {
    if (std::fabs(actual - expected) > 1e-9) {
        std::cerr << "FAIL: " << message << ": got " << actual << ", expected " << expected << '\n';
        std::exit(1);
    }
}

}  // namespace

int main() {
    using arsenal_mastery::power_multiplier;
    using arsenal_mastery::weapon_level;
    using arsenal_mastery::xp_threshold;

    expect_near(xp_threshold(1.0, 2), 750.0, "V2 base threshold");
    expect_near(xp_threshold(1.0, 5), 10000.0, "V5 base threshold");
    expect_near(xp_threshold(0.75, 3), 1687.5, "mid-game V3 threshold");
    expect_near(xp_threshold(0.5, 5), 5000.0, "late-game V5 threshold");
    expect_near(xp_threshold(1.0, 1), 0.0, "invalid low level threshold");
    expect_near(xp_threshold(1.0, 6), 0.0, "invalid high level threshold");

    expect(weapon_level(0.0, 1.0) == 1, "zero XP stays V1");
    expect(weapon_level(749.999, 1.0) == 1, "V2 threshold is inclusive");
    expect(weapon_level(750.0, 1.0) == 2, "V2 starts exactly at threshold");
    expect(weapon_level(2249.999, 1.0) == 2, "V3 threshold lower edge");
    expect(weapon_level(2250.0, 1.0) == 3, "V3 starts exactly at threshold");
    expect(weapon_level(5000.0, 1.0) == 4, "V4 starts exactly at threshold");
    expect(weapon_level(10000.0, 1.0) == 5, "V5 starts exactly at threshold");
    expect(weapon_level(1000000.0, 1.0) == 5, "progression caps at V5");

    expect(weapon_level(562.5, 0.75) == 2, "0.75-scale weapon reaches V2 correctly");
    expect(weapon_level(375.0, 0.5) == 2, "0.5-scale weapon reaches V2 correctly");
    expect(weapon_level(5000.0, 0.5) == 5, "0.5-scale weapon reaches V5 correctly");

    expect_near(power_multiplier(1), 1.0, "V1 power");
    expect_near(power_multiplier(2), 1.2, "V2 power");
    expect_near(power_multiplier(3), 1.5, "V3 power");
    expect_near(power_multiplier(4), 1.9, "V4 power");
    expect_near(power_multiplier(5), 2.5, "V5 power");
    expect_near(power_multiplier(99), 2.5, "power clamps above V5");
    expect_near(power_multiplier(-5), 1.0, "power clamps below V1");

    return 0;
}
