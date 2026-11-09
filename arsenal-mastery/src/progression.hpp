#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace arsenal {

constexpr std::size_t kWeaponCount = 16;
constexpr int kMaxWeaponLevel = 99;

struct WeaponDef {
    const char* key;
    const char* display_name;
    int item_id;
    double xp_scale;
    bool has_gold;
    double retail_gold_power;
    bool xp_mega;
    bool has_ammo;
};

const std::array<WeaponDef, kWeaponCount>& weapon_defs();
int weapon_from_item(int item_id);
int weapon_from_key(std::string_view key);
double ammo_capacity_factor(int weapon_level);

struct HitScore {
    double removed = 0.0;
    double xp = 0.0;
    bool killed = false;
};

// Arsenal Mastery awards XP from health actually removed, not raw/overkill damage. A killing hit
// also receives half of the victim's starting health as an XP bonus (both at xp_per_health).
HitScore score_hit(
    double effective_damage,
    double current_health,
    double starting_health,
    double xp_per_health = 10.0,
    double last_hit_bonus = 0.5
);

// Scores a hit after the game has processed it. Health delta is capped by the effective pending-hit
// damage so unrelated/environmental damage cannot be credited to the weapon.
HitScore score_resolved_hit(
    double health_before,
    double health_after,
    double starting_health,
    double effective_damage,
    bool killed_by_hit,
    double xp_per_health = 10.0,
    double last_hit_bonus = 0.5
);

struct Settings {
    double xp_rate = 1.0;
    bool level_power = true;
    bool gold_at_v5 = true;
    bool endless = false;
    bool portable_mega = true;  // OpenRAC fallback: unlock case weapons through XP too.
    int start_level = 0;  // 0 or 2..8
};

struct WeaponState {
    double xp = 0.0;
    double mega_xp = -1.0;  // < 0 = not Mega
    std::uint32_t kills = 0;
};

class Progression {
public:
    Progression();

    void reset();
    void set_settings(Settings settings);
    const Settings& settings() const { return settings_; }

    void set_new_game_plus(bool on);
    bool new_game_plus() const { return new_game_plus_; }

    const WeaponState& state(int weapon) const;
    WeaponState& mutable_state(int weapon);

    double xp_threshold(int weapon, int level) const;
    double mega_xp_threshold(int weapon) const;
    bool mega_allowed_by_xp(int weapon) const;
    bool is_mega(int weapon) const;
    bool set_mega(int weapon);

    int cap(int weapon) const;
    int level(int weapon) const;
    double progress(int weapon) const;
    double power(int weapon) const;

    int add_xp(int weapon, double raw_amount);
    void add_kill(int weapon);
    void refresh();

private:
    void apply_start(int weapon);
    void apply_mega_rules(int weapon);
    double after_mega_threshold(int level) const;

    Settings settings_{};
    bool new_game_plus_ = false;
    std::array<WeaponState, kWeaponCount> weapons_{};
};

class NanotechProgression {
public:
    void reset();
    void set_xp(double xp);
    double xp() const { return xp_; }
    void add_xp(double enemy_health_removed, double rate);

    int level(bool new_game_plus) const;
    double max_points(bool new_game_plus, int retail_max_orbs = 4) const;
    double progress(bool new_game_plus) const;
    double xp_for_level(int level) const;

private:
    double xp_ = 0.0;
};

}  // namespace arsenal
