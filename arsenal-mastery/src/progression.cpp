#include "progression.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace arsenal {
namespace {

constexpr std::array<WeaponDef, kWeaponCount> kWeapons = {{
    {"wrench", "Wrench", 8, 1.0, false, 1.0, false, false},
    {"suck_cannon", "Suck Cannon", 9, 1.0, true, 1.0, false, false},
    {"bomb_glove", "Bomb Glove", 10, 1.0, true, 1.0, false, true},
    {"devastator", "Devastator", 11, 0.75, true, 1.0, false, true},
    {"visibomb", "Visibomb Gun", 13, 0.75, false, 1.0, true, true},
    {"taunter", "Taunter", 14, 1.0, false, 1.0, true, false},
    {"blaster", "Blaster", 15, 1.0, true, 1.0, false, true},
    {"pyrocitor", "Pyrocitor", 16, 1.0, true, 2.0, false, true},
    {"mine_glove", "Mine Glove", 17, 0.75, true, 1.0, false, true},
    {"walloper", "Walloper", 18, 0.75, false, 1.0, true, false},
    {"tesla_claw", "Tesla Claw", 19, 0.75, true, 1.5, false, true},
    {"glove_of_doom", "Glove of Doom", 20, 0.5, true, 1.0, false, true},
    {"morph_o_ray", "Morph-o-Ray", 21, 0.5, true, 1.0, false, false},
    {"ryno", "R.Y.N.O.", 23, 0.5, false, 1.0, true, true},
    {"decoy", "Decoy Glove", 25, 0.5, true, 1.0, false, true},
    {"drone_device", "Drone Device", 24, 0.5, false, 1.0, true, true},
}};

constexpr std::array<double, 4> kXpTo = {{750.0, 2250.0, 5000.0, 10000.0}};
constexpr double kXpMegaCost = 15000.0;
constexpr double kAfterMegaV7 = 20000.0;
constexpr double kAfterMegaV8 = 40000.0;
constexpr double kEndlessGrowth = 1.15;
constexpr std::array<double, 5> kPower = {{1.0, 1.2, 1.5, 1.9, 2.5}};
constexpr double kPowerNoGoldV5 = 2.6;
constexpr double kMegaPower = 2.0;
constexpr double kGigaPower = 1.25;
constexpr double kOmegaPower = 1.25;
constexpr double kEndlessPower = 1.03;

constexpr std::array<std::pair<int, double>, 10> kNanoCurve = {{
    {10, 0.0}, {20, 2000.0}, {30, 5000.0}, {40, 9000.0}, {50, 14500.0},
    {60, 21500.0}, {70, 30000.0}, {80, 41000.0}, {90, 55000.0}, {100, 72500.0},
}};
constexpr double kNanoNgStep = 1100.0;
constexpr double kNanoXpPerHealth = 10.0 / 3.0;

bool valid_weapon(int weapon) {
    return weapon >= 0 && weapon < static_cast<int>(kWeaponCount);
}

}  // namespace

const std::array<WeaponDef, kWeaponCount>& weapon_defs() { return kWeapons; }

int weapon_from_item(int item_id) {
    for (std::size_t i = 0; i < kWeapons.size(); ++i) {
        if (kWeapons[i].item_id == item_id) return static_cast<int>(i);
    }
    return -1;
}

int weapon_from_key(std::string_view key) {
    for (std::size_t i = 0; i < kWeapons.size(); ++i) {
        if (key == kWeapons[i].key) return static_cast<int>(i);
    }
    return -1;
}

double ammo_capacity_factor(int weapon_level) {
    if (weapon_level >= 6) return 1.75;
    if (weapon_level >= 5) return 1.5;
    if (weapon_level >= 3) return 1.25;
    return 1.0;
}

HitScore score_hit(
    double effective_damage,
    double current_health,
    double starting_health,
    double xp_per_health,
    double last_hit_bonus
) {
    HitScore out;
    if (!(effective_damage > 0.0) || !(current_health > 0.0)) return out;

    const double hp = std::max(0.0, current_health);
    const double start = std::max(hp, starting_health);
    out.removed = std::min(effective_damage, hp);
    out.killed = effective_damage >= hp;
    out.xp = out.removed * std::max(0.0, xp_per_health);
    if (out.killed) {
        out.xp += start * std::max(0.0, xp_per_health) * std::max(0.0, last_hit_bonus);
    }
    return out;
}

HitScore score_resolved_hit(
    double health_before,
    double health_after,
    double starting_health,
    double effective_damage,
    bool killed_by_hit,
    double xp_per_health,
    double last_hit_bonus
) {
    HitScore out;
    if (!(health_before > 0.0) || !std::isfinite(health_before) || !std::isfinite(health_after)
        || !std::isfinite(effective_damage)) {
        return out;
    }

    const double before = std::max(0.0, health_before);
    const double after = std::clamp(health_after, 0.0, before);
    const double damage_cap = std::max(0.0, effective_damage);
    out.removed = std::min(before - after, damage_cap);
    out.killed = killed_by_hit;
    out.xp = out.removed * std::max(0.0, xp_per_health);
    if (out.killed) {
        const double start = std::max(before, starting_health);
        out.xp += start * std::max(0.0, xp_per_health) * std::max(0.0, last_hit_bonus);
    }
    return out;
}

Progression::Progression() { reset(); }

void Progression::reset() {
    weapons_ = {};
    new_game_plus_ = false;
    refresh();
}

void Progression::set_settings(Settings settings) {
    settings.xp_rate = std::clamp(settings.xp_rate, 0.0, 100.0);
    if (settings.start_level != 0) settings.start_level = std::clamp(settings.start_level, 2, 8);
    settings_ = settings;
    refresh();
}

void Progression::set_new_game_plus(bool on) {
    new_game_plus_ = on;
    refresh();
}

const WeaponState& Progression::state(int weapon) const {
    if (!valid_weapon(weapon)) throw std::out_of_range("weapon");
    return weapons_[static_cast<std::size_t>(weapon)];
}

WeaponState& Progression::mutable_state(int weapon) {
    if (!valid_weapon(weapon)) throw std::out_of_range("weapon");
    return weapons_[static_cast<std::size_t>(weapon)];
}

double Progression::xp_threshold(int weapon, int level_number) const {
    if (!valid_weapon(weapon) || level_number < 2 || level_number > 5) return 0.0;
    return kXpTo[static_cast<std::size_t>(level_number - 2)] * kWeapons[static_cast<std::size_t>(weapon)].xp_scale;
}

double Progression::mega_xp_threshold(int weapon) const {
    if (!valid_weapon(weapon)) return 0.0;
    const double scale = kWeapons[static_cast<std::size_t>(weapon)].xp_scale;
    return kXpTo[3] * scale + kXpMegaCost * scale;
}

bool Progression::mega_allowed_by_xp(int weapon) const {
    if (!valid_weapon(weapon)) return false;
    const auto& def = kWeapons[static_cast<std::size_t>(weapon)];
    if (!def.xp_mega && !settings_.portable_mega) return false;
    return new_game_plus_ || settings_.endless || settings_.start_level >= 6;
}

bool Progression::is_mega(int weapon) const {
    return valid_weapon(weapon) && weapons_[static_cast<std::size_t>(weapon)].mega_xp >= 0.0;
}

bool Progression::set_mega(int weapon) {
    if (!valid_weapon(weapon)) return false;
    auto& s = weapons_[static_cast<std::size_t>(weapon)];
    if (s.mega_xp >= 0.0) return false;
    s.xp = std::max(s.xp, xp_threshold(weapon, 5));
    s.mega_xp = s.xp;
    return true;
}

double Progression::after_mega_threshold(int level_number) const {
    if (level_number <= 6) return 0.0;
    if (level_number == 7) return kAfterMegaV7;
    if (level_number == 8) return kAfterMegaV8;
    double cumulative = kAfterMegaV8;
    double step = kAfterMegaV8 - kAfterMegaV7;
    for (int n = 9; n <= std::min(level_number, kMaxWeaponLevel); ++n) {
        step *= kEndlessGrowth;
        cumulative += step;
    }
    return cumulative;
}

int Progression::cap(int weapon) const {
    if (!valid_weapon(weapon)) return 1;
    const bool mega = is_mega(weapon);
    if (settings_.endless) return mega ? kMaxWeaponLevel : 5;
    if (mega && (new_game_plus_ || settings_.start_level >= 6)) return 8;
    return 5;
}

int Progression::level(int weapon) const {
    if (!valid_weapon(weapon)) return 1;
    const auto& s = weapons_[static_cast<std::size_t>(weapon)];
    int out = 1;
    for (int n = 2; n <= 5; ++n) {
        if (s.xp >= xp_threshold(weapon, n)) out = n;
        else break;
    }
    if (out == 5 && s.mega_xp >= 0.0) {
        out = 6;
        const double relative = s.xp - s.mega_xp;
        const double scale = kWeapons[static_cast<std::size_t>(weapon)].xp_scale;
        for (int n = 7; n <= kMaxWeaponLevel; ++n) {
            if (relative >= after_mega_threshold(n) * scale) out = n;
            else break;
        }
    }
    return std::min(out, cap(weapon));
}

double Progression::progress(int weapon) const {
    if (!valid_weapon(weapon)) return 0.0;
    const auto& s = weapons_[static_cast<std::size_t>(weapon)];
    const int l = level(weapon);
    if (l >= cap(weapon)) {
        if (l == 5 && !is_mega(weapon) && mega_allowed_by_xp(weapon)) {
            const double lo = xp_threshold(weapon, 5);
            const double hi = mega_xp_threshold(weapon);
            return std::clamp((s.xp - lo) / std::max(1.0, hi - lo), 0.0, 1.0);
        }
        return 1.0;
    }
    if (l < 5) {
        const double lo = l == 1 ? 0.0 : xp_threshold(weapon, l);
        const double hi = xp_threshold(weapon, l + 1);
        return std::clamp((s.xp - lo) / std::max(1.0, hi - lo), 0.0, 1.0);
    }
    if (l == 5) return 1.0;
    const double relative = s.xp - s.mega_xp;
    const double scale = kWeapons[static_cast<std::size_t>(weapon)].xp_scale;
    const double lo = l == 6 ? 0.0 : after_mega_threshold(l) * scale;
    const double hi = after_mega_threshold(l + 1) * scale;
    return std::clamp((relative - lo) / std::max(1.0, hi - lo), 0.0, 1.0);
}

double Progression::power(int weapon) const {
    if (!valid_weapon(weapon) || !settings_.level_power) return 1.0;
    const int l = level(weapon);
    if (l <= 4) return kPower[static_cast<std::size_t>(l - 1)];

    const auto& def = kWeapons[static_cast<std::size_t>(weapon)];
    // OpenRAC does not currently expose the retail Gold Weapon Case spare-bolt hook.
    // Do not flip the game's gold flag here: doing so would make the retail game consume gold
    // bolts for a free V5 upgrade. Instead use the original Arsenal Mastery target V5 power
    // directly; the visual/retail-gold behaviour remains the game's own when actually owned.
    const bool gold_equivalent = def.has_gold && settings_.gold_at_v5;
    double p = gold_equivalent ? kPower[4] : kPowerNoGoldV5;
    if (l >= 6) p *= kMegaPower;
    if (l >= 7) p *= kGigaPower;
    if (l >= 8) p *= kOmegaPower;
    if (l >= 9) p *= std::pow(kEndlessPower, l - 8);
    return p;
}

int Progression::add_xp(int weapon, double raw_amount) {
    if (!valid_weapon(weapon) || !(raw_amount > 0.0)) return valid_weapon(weapon) ? level(weapon) : 1;
    auto& s = weapons_[static_cast<std::size_t>(weapon)];
    s.xp += raw_amount * settings_.xp_rate;
    apply_mega_rules(weapon);
    return level(weapon);
}

void Progression::add_kill(int weapon) {
    if (valid_weapon(weapon)) ++weapons_[static_cast<std::size_t>(weapon)].kills;
}

void Progression::apply_mega_rules(int weapon) {
    if (!valid_weapon(weapon)) return;
    auto& s = weapons_[static_cast<std::size_t>(weapon)];
    if (s.mega_xp < 0.0 && mega_allowed_by_xp(weapon) && s.xp >= mega_xp_threshold(weapon)) {
        s.mega_xp = mega_xp_threshold(weapon);
    }
}

void Progression::apply_start(int weapon) {
    if (!valid_weapon(weapon) || settings_.start_level < 2) return;
    auto& s = weapons_[static_cast<std::size_t>(weapon)];
    const int target = std::min(settings_.start_level, 8);
    if (target <= 5) {
        s.xp = std::max(s.xp, xp_threshold(weapon, target));
        return;
    }
    if (s.mega_xp < 0.0) s.mega_xp = mega_xp_threshold(weapon);
    const double scale = kWeapons[static_cast<std::size_t>(weapon)].xp_scale;
    const double needed = s.mega_xp + (target >= 7 ? after_mega_threshold(target) * scale : 0.0);
    s.xp = std::max(s.xp, needed);
}

void Progression::refresh() {
    for (int weapon = 0; weapon < static_cast<int>(kWeaponCount); ++weapon) {
        apply_start(weapon);
        apply_mega_rules(weapon);
    }
}

void NanotechProgression::reset() { xp_ = 0.0; }

void NanotechProgression::set_xp(double xp) { xp_ = std::max(0.0, xp); }

void NanotechProgression::add_xp(double enemy_health_removed, double rate) {
    if (enemy_health_removed > 0.0 && rate > 0.0) xp_ += enemy_health_removed * kNanoXpPerHealth * rate;
}

double NanotechProgression::xp_for_level(int level_number) const {
    const int hp = 10 + std::max(1, level_number) - 1;
    if (hp <= 10) return 0.0;
    for (std::size_t i = 1; i < kNanoCurve.size(); ++i) {
        if (hp <= kNanoCurve[i].first) {
            const auto a = kNanoCurve[i - 1];
            const auto b = kNanoCurve[i];
            const double value = a.second + (b.second - a.second) * (hp - a.first) / (b.first - a.first);
            return std::floor(value + 0.5);
        }
    }
    return kNanoCurve.back().second + (hp - kNanoCurve.back().first) * kNanoNgStep;
}

int NanotechProgression::level(bool new_game_plus) const {
    const int max_hp = new_game_plus ? 200 : 100;
    const int max_level = max_hp - 10 + 1;
    int out = 1;
    while (out < max_level && xp_ >= xp_for_level(out + 1)) ++out;
    return out;
}

double NanotechProgression::max_points(bool new_game_plus, int retail_max_orbs) const {
    const int base = 10 + level(new_game_plus) - 1;
    const int orbs = std::clamp(retail_max_orbs, 1, 16);
    return static_cast<double>(base) * static_cast<double>(orbs) / 4.0;
}

double NanotechProgression::progress(bool new_game_plus) const {
    const int l = level(new_game_plus);
    const int max_hp = new_game_plus ? 200 : 100;
    const int max_level = max_hp - 10 + 1;
    if (l >= max_level) return 1.0;
    const double lo = xp_for_level(l);
    const double hi = xp_for_level(l + 1);
    return std::clamp((xp_ - lo) / std::max(1.0, hi - lo), 0.0, 1.0);
}

}  // namespace arsenal
