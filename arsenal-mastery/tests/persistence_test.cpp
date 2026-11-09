#include "persistence.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

using namespace arsenal;

static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) <= eps; }

int main() {
    const int blaster = weapon_from_key("blaster");
    const int ryno = weapon_from_key("ryno");
    assert(blaster >= 0 && ryno >= 0);

    Progression source;
    Settings settings;
    settings.endless = true;
    source.set_settings(settings);
    source.set_new_game_plus(true);
    source.mutable_state(blaster).xp = 12345.5;
    source.mutable_state(blaster).mega_xp = 10000.0;
    source.mutable_state(blaster).kills = 27;
    source.mutable_state(ryno).xp = 25001.25;
    source.mutable_state(ryno).mega_xp = 12500.0;
    source.mutable_state(ryno).kills = 9;
    source.refresh();
    NanotechProgression nano;
    nano.set_xp(4321.75);

    std::ostringstream encoded;
    assert(write_progress(encoded, source, nano));
    assert(encoded.str().find("arsenal-mastery 3\n") == 0);

    PersistedState persisted;
    std::istringstream input(encoded.str());
    assert(read_progress(input, persisted));
    assert(persisted.source_version == 3);

    Progression restored;
    restored.set_settings(settings);
    NanotechProgression restored_nano;
    apply_progress(persisted, restored, restored_nano);
    assert(restored.new_game_plus());
    assert(near(restored.state(blaster).xp, 12345.5));
    assert(near(restored.state(blaster).mega_xp, 10000.0));
    assert(restored.state(blaster).kills == 27);
    assert(near(restored.state(ryno).xp, 25001.25));
    assert(near(restored_nano.xp(), 4321.75));

    std::istringstream old(
        "arsenal-mastery 1\n"
        "ng-plus 1\n"
        "blaster 900\n"
        "unknown_future_weapon 123\n"
        "ryno 456.5\n"
    );
    PersistedState v1;
    assert(read_progress(old, v1));
    assert(v1.source_version == 1);
    assert(v1.new_game_plus);
    assert(near(v1.weapons[static_cast<std::size_t>(blaster)].xp, 900.0));
    assert(near(v1.weapons[static_cast<std::size_t>(ryno)].xp, 456.5));
    assert(v1.weapons[static_cast<std::size_t>(blaster)].mega_xp < 0.0);
    assert(near(v1.nanotech_xp, 0.0));

    PersistedState untouched;
    untouched.new_game_plus = true;
    untouched.nanotech_xp = 99.0;
    std::istringstream bad("arsenal-mastery 3\nng-plus 0\nnanotech-xp nope\n");
    assert(!read_progress(bad, untouched));
    assert(untouched.new_game_plus);
    assert(near(untouched.nanotech_xp, 99.0));

    std::istringstream unsupported("arsenal-mastery 99\nng-plus 0\n");
    assert(!read_progress(unsupported, untouched));

    std::istringstream bad_mega(
        "arsenal-mastery 3\nng-plus 0\nnanotech-xp 0\nweapon blaster 100 200 1\n"
    );
    assert(!read_progress(bad_mega, untouched));

    std::istringstream huge_kills(
        "arsenal-mastery 3\nng-plus 0\nnanotech-xp 0\nweapon blaster 100 -1 999999999999\n"
    );
    PersistedState capped;
    assert(read_progress(huge_kills, capped));
    assert(capped.weapons[static_cast<std::size_t>(blaster)].kills
           == std::numeric_limits<std::uint32_t>::max());

    std::cout << "persistence tests passed\n";
    return 0;
}
