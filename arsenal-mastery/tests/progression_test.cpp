#include "progression.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace arsenal;

static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) <= eps; }

int main() {
    Progression p;
    Settings s;
    p.set_settings(s);

    const int blaster = weapon_from_key("blaster");
    const int ryno = weapon_from_key("ryno");
    const int pyro = weapon_from_key("pyrocitor");
    assert(blaster >= 0 && ryno >= 0 && pyro >= 0);

    assert(p.level(blaster) == 1);
    p.add_xp(blaster, 749.0);
    assert(p.level(blaster) == 1);
    p.add_xp(blaster, 1.0);
    assert(p.level(blaster) == 2);
    p.mutable_state(blaster).xp = 10000.0;
    assert(p.level(blaster) == 5);
    assert(near(p.power(blaster), 2.5));

    p.mutable_state(pyro).xp = 10000.0;
    assert(p.level(pyro) == 5);
    assert(near(p.power(pyro), 2.5));

    p.set_new_game_plus(true);
    p.mutable_state(ryno).xp = p.mega_xp_threshold(ryno);
    p.refresh();
    assert(p.is_mega(ryno));
    assert(p.level(ryno) == 6);
    const double rscale = weapon_defs()[static_cast<std::size_t>(ryno)].xp_scale;
    p.mutable_state(ryno).xp = p.state(ryno).mega_xp + 20000.0 * rscale;
    assert(p.level(ryno) == 7);
    p.mutable_state(ryno).xp = p.state(ryno).mega_xp + 40000.0 * rscale;
    assert(p.level(ryno) == 8);

    Progression portable_case;
    Settings portable_settings;
    portable_case.set_settings(portable_settings);
    portable_case.set_new_game_plus(true);
    portable_case.mutable_state(blaster).xp = portable_case.mega_xp_threshold(blaster);
    portable_case.refresh();
    assert(portable_case.is_mega(blaster));
    assert(portable_case.level(blaster) == 6);

    s.endless = true;
    p.set_settings(s);
    p.mutable_state(ryno).xp = p.state(ryno).mega_xp + 40000.0 * rscale + 20000.0 * 1.15 * rscale;
    assert(p.level(ryno) == 9);
    assert(p.cap(ryno) == 99);

    Progression sandbox;
    s = {};
    s.start_level = 6;
    sandbox.set_settings(s);
    assert(sandbox.level(blaster) == 6);
    assert(sandbox.is_mega(blaster));

    assert(near(ammo_capacity_factor(1), 1.0));
    assert(near(ammo_capacity_factor(3), 1.25));
    assert(near(ammo_capacity_factor(5), 1.5));
    assert(near(ammo_capacity_factor(6), 1.75));
    assert(near(ammo_capacity_factor(99), 1.75));

    const HitScore one_shot = score_hit(10.0, 3.0, 3.0);
    assert(near(one_shot.removed, 3.0));
    assert(one_shot.killed);
    assert(near(one_shot.xp, 45.0));

    const HitScore finisher = score_hit(5.0, 3.0, 10.0);
    assert(near(finisher.removed, 3.0));
    assert(finisher.killed);
    assert(near(finisher.xp, 80.0));

    const HitScore normal_hit = score_hit(2.5, 8.0, 10.0);
    assert(near(normal_hit.removed, 2.5));
    assert(!normal_hit.killed);
    assert(near(normal_hit.xp, 25.0));

    const HitScore resolved = score_resolved_hit(10.0, 7.0, 10.0, 5.0, false);
    assert(near(resolved.removed, 3.0));
    assert(!resolved.killed);
    assert(near(resolved.xp, 30.0));

    const HitScore resolved_kill = score_resolved_hit(3.0, 0.0, 3.0, 10.0, true);
    assert(near(resolved_kill.removed, 3.0));
    assert(resolved_kill.killed);
    assert(near(resolved_kill.xp, 45.0));

    const HitScore environment_guard = score_resolved_hit(10.0, 0.0, 10.0, 2.0, false);
    assert(near(environment_guard.removed, 2.0));
    assert(!environment_guard.killed);
    assert(near(environment_guard.xp, 20.0));

    Progression strict_case;
    Settings strict_settings;
    strict_settings.portable_mega = false;
    strict_case.set_settings(strict_settings);
    strict_case.set_new_game_plus(true);
    strict_case.mutable_state(blaster).xp = strict_case.mega_xp_threshold(blaster) * 2.0;
    strict_case.refresh();
    assert(!strict_case.is_mega(blaster));
    assert(strict_case.level(blaster) == 5);

    NanotechProgression n;
    assert(n.level(false) == 1);
    assert(near(n.max_points(false, 5), 12.5));
    n.set_xp(2000.0);
    assert(near(n.max_points(false, 4), 20.0));
    assert(near(n.max_points(false, 5), 25.0));
    n.set_xp(72500.0);
    assert(near(n.max_points(false, 4), 100.0));
    n.set_xp(72500.0 + 1100.0);
    assert(near(n.max_points(true, 4), 101.0));

    std::cout << "progression tests passed\n";
    return 0;
}
