// SPDX-License-Identifier: MIT
// Copyright (c) 2026 AsobiCodes
// OpenRAC port: Moritz Wallinger
//
// Arsenal Mastery for OpenRAC. This native port keeps its own progression sidecar per save slot and
// uses only OpenRAC's published mod interface plus the same documented RAC1 globals already used by
// OpenRAC's first-party mod examples. It deliberately does not patch machine code.
#include "progression.hpp"
#include "persistence.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "openrac/mod.hpp"

namespace {

using arsenal::NanotechProgression;
using arsenal::Progression;
using arsenal::Settings;
using arsenal::WeaponDef;
using openrac::gaddr;

constexpr const char* kModVersion = "0.4.3";
constexpr std::uint64_t kFlushTicks = 60;
constexpr std::uint64_t kVictimLifetimeTicks = 1800;
constexpr double kHardFactor = 1.3;
constexpr double kNgPlusFactor = 2.5;

struct Disc {
    const char* version;
    std::uint32_t hero;
    std::uint32_t hero_health;
    std::uint32_t max_health;
    std::uint32_t ammo;
    std::uint32_t item_in_hand;
    std::uint32_t held_moby;
    std::uint32_t item_table;
    std::uint32_t hit_ring;
    std::uint32_t challenge;
    std::uint32_t gold_flags;
};

constexpr Disc kDiscs[] = {
    {"pal", 0x0013F450u, 0x001416F8u, 0x0015EEA0u, 0x0013D530u, 0x00140508u,
     0x001404E0u, 0x001C43B0u, 0x00178200u, 0x0015EF20u, 0x0013E620u},
    {"ntsc", 0x0013F350u, 0x001415F8u, 0x0015EDA0u, 0x0013D428u, 0x00140408u,
     0x001403E0u, 0x001C40B0u, 0x00178100u, 0x0015EE20u, 0x0013E520u},
};

struct GetHit {
    static constexpr const char* id = "rac1.combat.get_hit";
    static constexpr const char* name = "Moby_GetHitMessage";
    static constexpr const char* signature = "u32(u32,i32,i32)";
    using fn = std::uint32_t (*)(std::uint32_t, std::int32_t, std::int32_t);
};
struct TakeDamage {
    static constexpr const char* id = "rac1.player.take_damage";
    static constexpr const char* name = "Hero_TakeDamage";
    static constexpr const char* signature = "void(i32)";
    using fn = void (*)(std::int32_t);
};
struct RecordDeath {
    static constexpr const char* id = "rac1.moby.record_death";
    static constexpr const char* name = "Moby_RecordDeath";
    static constexpr const char* signature = "void(u32,i32,i32)";
    using fn = void (*)(std::uint32_t, std::int32_t, std::int32_t);
};
struct LoadGameplay {
    static constexpr const char* id = "rac1.level.load_gameplay";
    static constexpr const char* name = "Level_LoadGameplay";
    static constexpr const char* signature = "u32(i32)";
    using fn = std::uint32_t (*)(std::int32_t);
};
struct NewGame {
    static constexpr const char* id = "rac1.save.new_game";
    static constexpr const char* name = "Save_NewGame";
    static constexpr const char* signature = "void()";
    using fn = void (*)();
};
struct NewGamePlus {
    static constexpr const char* id = "rac1.save.new_game_plus";
    static constexpr const char* name = "Save_NewGamePlus";
    static constexpr const char* signature = "void(i32)";
    using fn = void (*)(std::int32_t);
};

struct Attacker {
    std::int16_t moby_class;
    int weapon;
};

// Projectile / held-object attribution from RAC-RS Arsenal Mastery power.luau.
constexpr std::array<Attacker, 27> kAttackers = {{
    {121, 2}, {192, 2}, {74, 8}, {190, 8}, {305, 6}, {168, 6}, {457, 13}, {454, 13},
    {186, 11}, {230, 11}, {229, 11}, {172, 4}, {163, 4}, {153, 3}, {157, 3},
    {176, 7}, {177, 10}, {180, 9}, {175, 5}, {185, 12}, {562, 14}, {849, 1},
    {483, 15}, {479, 15}, {203, 14}, {1900, 14}, {71, 0},
}};

// Held weapon classes used when Ratchet himself is the hit attacker. This mirrors RAC-RS instead
// of trusting only item_in_hand, which can lag during wrench/glove transitions.
constexpr std::array<Attacker, 16> kHeldWeapons = {{
    {71, 0}, {849, 1}, {192, 2}, {157, 3}, {163, 4}, {175, 5}, {168, 6}, {176, 7},
    {190, 8}, {180, 9}, {177, 10}, {229, 11}, {185, 12}, {454, 13}, {562, 14}, {483, 15},
}};

// Enemy classes explicitly accepted by RAC-RS Arsenal Mastery. Keeping this catalogue avoids
// awarding XP for crates, NPCs and other damageable objects.
constexpr std::array<std::int16_t, 76> kEnemyClasses = {{
    8, 28, 30, 31, 36, 44, 52, 63, 101, 111, 170, 217, 231, 233, 238, 252, 253,
    294, 326, 333, 340, 347, 388, 427, 455, 459, 491, 541, 563, 572, 573, 574,
    577, 578, 580, 612, 623, 625, 631, 638, 643, 668, 749, 827, 865, 866, 871,
    933, 1023, 1041, 1048, 1051, 1059, 1068, 1106, 1110, 1112, 1126, 1172,
    1196, 1202, 1219, 1229, 1231, 1246, 1261, 1262, 1269, 1356, 1382, 1422,
    1440, 1454, 1805, 1885, 1906,
}};

constexpr std::array<double, 19> kRebalance = {{
    1.0, 1.0, 1.1, 1.2, 1.3, 1.45, 1.6, 1.75, 1.9, 2.0,
    2.1, 2.2, 2.3, 2.35, 2.4, 2.45, 2.5, 2.5, 2.5,
}};
constexpr std::array<double, 19> kNanotechDamage = {{
    3, 4, 6, 8, 10, 11, 12, 13, 14, 15, 16, 17, 18, 18, 19, 20, 20, 20, 20,
}};

constexpr std::uint32_t kHeroMoby = 0x2080u;
constexpr std::uint32_t kMobyClass = 0xA6u;
constexpr std::uint32_t kMobyClassPtr = 0x24u;
constexpr std::uint32_t kClassType = 0x46u;
constexpr std::uint32_t kMobyPvar = 0x78u;
constexpr std::uint32_t kEnemyHealth = 0x20u;
constexpr std::uint32_t kBombOwner = 0x50u;
constexpr std::uint32_t kHitAttacker = 0x20u;
constexpr std::uint32_t kHitDamage = 0x2Cu;
constexpr std::uint32_t kHitSize = 0x40u;
constexpr int kHitCount = 64;
constexpr std::uint32_t kItemSize = 0x18u;
constexpr std::uint32_t kItemMaxAmmo = 0x0Eu;

struct SeenHit {
    std::uint32_t victim = 0;
    std::uint32_t attacker = 0;
    float raw_damage = 0.0f;
    float scaled_damage = 0.0f;
    std::uint64_t tick = 0;
};

struct VictimProgress {
    std::int16_t moby_class = -1;
    double starting_health = 0.0;
    double predicted_health = 0.0;
    std::uint64_t tick = 0;
};

struct PendingScore {
    int weapon = -1;
    double health_before = 0.0;
    double starting_health = 0.0;
    double effective_damage = 0.0;
    float raw_damage = 0.0f;
    float scaled_damage = 0.0f;
    double balance = 1.0;
    std::uint64_t tick = 0;
};

openrac::Mod* sMod = nullptr;
const Disc* sDisc = nullptr;
Progression sProgression;
NanotechProgression sNanotech;
std::array<SeenHit, kHitCount> sSeen{};
std::unordered_map<std::uint32_t, VictimProgress> sVictims;
std::unordered_map<std::uint32_t, PendingScore> sPendingScores;
std::array<int, arsenal::kWeaponCount> sRetailAmmoCap{};
std::array<int, arsenal::kWeaponCount> sAppliedAmmoLevel{};

std::uint64_t sTick = 0;
std::uint64_t sLastFlushTick = 0;
int sSaveSlot = -1;
bool sDirty = false;
bool sFreshNewGame = false;
std::string sDataDir;

std::string sRebalanceMode = "standard";
std::string sAmmoMode = "normal";
double sWeaponDamage = 1.0;
double sEnemyDamage = 1.0;
double sNanotechXpRate = 1.0;
bool sNanotechEnabled = true;
bool sXpDisplay = true;
bool sTrace = false;

double sEnemyDamageCarry = 0.0;
std::uint64_t sHeroPaidHits = 0;
double sNanoPool = -1.0;
double sNanoMax = 10.0;
int sNanoLastOrbs = -1;
int sLastNanoLevel = 1;
bool sHudVisible = false;
int sUiPageIndex = 0;

const auto& weapons() { return arsenal::weapon_defs(); }

template <class T>
T get(gaddr at) {
    return sMod->memory().read<T>(at);
}

template <class T>
void put(gaddr at, T value) {
    sMod->memory().write<T>(at, value);
}

gaddr exe(std::uint32_t address) { return sMod->relocate(address); }
gaddr level0(std::uint32_t address) { return sMod->relocate_level(0, address); }
gaddr hero() { return exe(sDisc->hero); }

void reset_runtime_state() {
    sSeen.fill(SeenHit{});
    sVictims.clear();
    sPendingScores.clear();
    sRetailAmmoCap.fill(-1);
    sAppliedAmmoLevel.fill(-1);
    sEnemyDamageCarry = 0.0;
    sHeroPaidHits = 0;
    sNanoPool = -1.0;
    sNanoMax = 10.0;
    sNanoLastOrbs = -1;
    sLastNanoLevel = sNanotech.level(sProgression.new_game_plus());
}

std::filesystem::path progress_path(int slot) {
    return std::filesystem::path(sDataDir) / ("slot-" + std::to_string(slot) + ".txt");
}

void mark_dirty() { sDirty = true; }

void save_progress(int slot) {
    if (slot < 0 || sDataDir.empty()) return;

    std::error_code ec;
    std::filesystem::create_directories(sDataDir, ec);
    const auto final_path = progress_path(slot);
    const auto temp_path = final_path.string() + ".tmp";
    std::ofstream out(temp_path, std::ios::trunc);
    if (!out) {
        sMod->warn("Arsenal Mastery: cannot write progression for slot " + std::to_string(slot));
        return;
    }

    const bool wrote = arsenal::write_progress(out, sProgression, sNanotech);
    out.close();
    if (!wrote || !out) {
        std::filesystem::remove(temp_path, ec);
        sMod->warn("Arsenal Mastery: failed while writing progression for slot " + std::to_string(slot));
        return;
    }

    std::filesystem::rename(temp_path, final_path, ec);
    if (ec) {
        // Windows cannot replace an existing file with rename(). Fall back to remove+rename only
        // after the complete temporary file has been closed successfully.
        std::error_code remove_ec;
        std::filesystem::remove(final_path, remove_ec);
        ec.clear();
        std::filesystem::rename(temp_path, final_path, ec);
    }
    if (ec) {
        std::filesystem::remove(temp_path, ec);
        sMod->warn("Arsenal Mastery: cannot commit progression for slot " + std::to_string(slot));
        return;
    }

    sDirty = false;
    sLastFlushTick = sTick;
}

void load_progress(int slot) {
    sProgression.reset();
    sNanotech.reset();
    reset_runtime_state();

    if (slot < 0 || sDataDir.empty()) {
        sDirty = false;
        return;
    }

    std::ifstream in(progress_path(slot));
    if (!in) {
        sDirty = false;
        return;
    }

    arsenal::PersistedState saved;
    if (!arsenal::read_progress(in, saved)) {
        sMod->warn("Arsenal Mastery: unsupported/damaged progression file for slot " + std::to_string(slot));
        sDirty = false;
        return;
    }

    arsenal::apply_progress(saved, sProgression, sNanotech);
    reset_runtime_state();
    sDirty = saved.source_version != arsenal::kProgressFileVersion;  // migrate v1 on next flush.
    sMod->info("Arsenal Mastery: loaded progression for save slot " + std::to_string(slot));
}

void sync_save_slot() {
    if (!sMod->has_api_11()) return;
    const int current = sMod->save_slot_current();
    if (current == sSaveSlot) return;

    if (sSaveSlot >= 0 && sDirty) save_progress(sSaveSlot);
    sSaveSlot = current;
    if (current < 0) return;

    if (sFreshNewGame) {
        sFreshNewGame = false;
        sDirty = true;
        save_progress(current);
    } else {
        load_progress(current);
    }
}

template <class Table>
int weapon_from_class(std::int16_t cls, const Table& table) {
    for (const auto& entry : table) {
        if (entry.moby_class == cls) return entry.weapon;
    }
    return -1;
}

int weapon_from_attacker(std::uint32_t attacker) {
    if (attacker == 0) return -1;
    const std::uint32_t hero_moby = get<std::uint32_t>(hero() + kHeroMoby);
    const std::int16_t cls = get<std::int16_t>(attacker + kMobyClass);

    if (attacker == hero_moby || cls == 0) {
        const std::uint32_t held = get<std::uint32_t>(exe(sDisc->held_moby));
        if (held != 0) {
            const int by_class = weapon_from_class(get<std::int16_t>(held + kMobyClass), kHeldWeapons);
            if (by_class >= 0) return by_class;
        }
        return arsenal::weapon_from_item(get<std::int32_t>(exe(sDisc->item_in_hand)));
    }

    // Class 121 is normally a Bomb Glove bomb, but Visibomb's split projectiles reuse it and keep
    // their Visibomb projectile as owner. Mirror RAC-RS's owner check before the static class map.
    if (cls == 121) {
        const std::uint32_t pvar = get<std::uint32_t>(attacker + kMobyPvar);
        if (pvar != 0) {
            const std::uint32_t owner = get<std::uint32_t>(pvar + kBombOwner);
            if (owner != 0 && get<std::int16_t>(owner + kMobyClass) == 163) {
                return arsenal::weapon_from_key("visibomb");
            }
        }
    }

    return weapon_from_class(cls, kAttackers);
}

bool is_enemy(std::uint32_t moby) {
    if (moby == 0 || moby == get<std::uint32_t>(hero() + kHeroMoby)) return false;
    const std::int16_t cls = get<std::int16_t>(moby + kMobyClass);
    if (std::find(kEnemyClasses.begin(), kEnemyClasses.end(), cls) != kEnemyClasses.end()) return true;

    // RAC1's class record labels type 5 as enemy, 9 as crate and 11 as another targetable object.
    // This catches enemy classes added to the decomp/port after the original Arsenal Mastery table
    // without giving XP for crates or NPC targets.
    const std::uint32_t class_ptr = get<std::uint32_t>(moby + kMobyClassPtr);
    return class_ptr != 0 && get<std::int16_t>(class_ptr + kClassType) == 5;
}

double rebalance_factor() {
    if (sRebalanceMode == "off") return 1.0;
    const int level = sMod->loaded_level();
    double factor = 1.0;
    if (level >= 0 && level < static_cast<int>(kRebalance.size())) {
        factor = kRebalance[static_cast<std::size_t>(level)];
    }
    if (sRebalanceMode == "hard") factor *= kHardFactor;
    if (sProgression.new_game_plus()) factor *= kNgPlusFactor;
    return factor;
}

bool actual_gold_weapon(int weapon) {
    if (weapon < 0 || weapon >= static_cast<int>(weapons().size())) return false;
    const int item = weapons()[static_cast<std::size_t>(weapon)].item_id;
    return get<std::uint8_t>(exe(sDisc->gold_flags) + static_cast<std::uint32_t>(item)) != 0;
}

double weapon_power(int weapon) {
    double power = sProgression.power(weapon);
    const auto& def = weapons()[static_cast<std::size_t>(weapon)];
    // Retail Pyrocitor/Tesla gold versions multiply their own hit before Arsenal Mastery sees it.
    // Divide that part out only when the player actually owns the retail gold version; the earlier
    // port never grants that flag for free, so it cannot accidentally spend the player's gold bolts.
    if (sProgression.settings().gold_at_v5 && sProgression.level(weapon) >= 5
        && actual_gold_weapon(weapon) && def.retail_gold_power > 1.0) {
        power /= def.retail_gold_power;
    }
    return power;
}

bool repeated_hit(std::uint32_t hit, std::uint32_t victim, std::uint32_t attacker, float current_damage) {
    const gaddr ring = level0(sDisc->hit_ring);
    if (hit < ring || (hit - ring) % kHitSize != 0) return false;
    const std::uint32_t index = (hit - ring) / kHitSize;
    if (index >= kHitCount) return false;
    const SeenHit& seen = sSeen[index];
    if (seen.victim != victim || seen.attacker != attacker
        || std::fabs(seen.scaled_damage - current_damage) >= 0.0001f) {
        return false;
    }

    // When the mod changed the damage, the modified value itself identifies a repeat until this
    // ring slot is overwritten. If retail damage and scaled damage are identical, keep a short
    // tick guard so a later independent equal-strength hit in the same ring slot is not suppressed.
    if (std::fabs(seen.raw_damage - seen.scaled_damage) >= 0.0001f) return true;
    return sTick <= seen.tick + 2;
}

void remember_hit(
    std::uint32_t hit,
    std::uint32_t victim,
    std::uint32_t attacker,
    float raw_damage,
    float scaled_damage
) {
    const gaddr ring = level0(sDisc->hit_ring);
    if (hit < ring || (hit - ring) % kHitSize != 0) return;
    const std::uint32_t index = (hit - ring) / kHitSize;
    if (index < kHitCount) sSeen[index] = {victim, attacker, raw_damage, scaled_damage, sTick};
}

std::optional<double> enemy_health(std::uint32_t victim) {
    const std::uint32_t pvar = get<std::uint32_t>(victim + kMobyPvar);
    if (pvar == 0) return std::nullopt;
    const float health = get<float>(pvar + kEnemyHealth);
    if (!std::isfinite(health) || health < 0.0f) return std::nullopt;
    return static_cast<double>(health);
}

double starting_health_for(std::uint32_t victim, double current_health) {
    const std::int16_t cls = get<std::int16_t>(victim + kMobyClass);
    auto& v = sVictims[victim];
    const bool reset = v.moby_class != cls || !(v.starting_health > 0.0)
                       || current_health > v.predicted_health + 0.001;
    if (reset) {
        v.moby_class = cls;
        v.starting_health = current_health;
    }
    v.predicted_health = current_health;
    v.tick = sTick;
    return v.starting_health;
}

void note_predicted_health(std::uint32_t victim, double remaining) {
    auto it = sVictims.find(victim);
    if (it == sVictims.end()) return;
    it->second.predicted_health = std::max(0.0, remaining);
    it->second.tick = sTick;
}

int current_ammo_cap(int weapon) {
    if (weapon < 0 || weapon >= static_cast<int>(weapons().size())) return 0;
    const auto& def = weapons()[static_cast<std::size_t>(weapon)];
    if (!def.has_ammo) return 0;
    const int retail = sRetailAmmoCap[static_cast<std::size_t>(weapon)];
    if (retail <= 0) return 0;
    const int level = sProgression.settings().level_power ? sProgression.level(weapon) : 1;
    return std::max(1, static_cast<int>(std::lround(retail * arsenal::ammo_capacity_factor(level))));
}

void snapshot_retail_ammo_caps() {
    sRetailAmmoCap.fill(-1);
    sAppliedAmmoLevel.fill(-1);
    if (sMod->loaded_level() < 0) return;
    const gaddr table = level0(sDisc->item_table);
    for (std::size_t i = 0; i < weapons().size(); ++i) {
        if (!weapons()[i].has_ammo) continue;
        const auto item = static_cast<std::uint32_t>(weapons()[i].item_id);
        sRetailAmmoCap[i] = static_cast<int>(get<std::uint16_t>(table + item * kItemSize + kItemMaxAmmo));
    }
}

void apply_ammo_capacity(int weapon, bool force = false) {
    if (weapon < 0 || weapon >= static_cast<int>(weapons().size()) || sMod->loaded_level() < 0) return;
    const auto& def = weapons()[static_cast<std::size_t>(weapon)];
    if (!def.has_ammo || sRetailAmmoCap[static_cast<std::size_t>(weapon)] <= 0) return;

    const int level = sProgression.settings().level_power ? sProgression.level(weapon) : 1;
    if (!force && sAppliedAmmoLevel[static_cast<std::size_t>(weapon)] == level) return;
    const int cap = current_ammo_cap(weapon);
    if (cap <= 0) return;

    const auto item = static_cast<std::uint32_t>(def.item_id);
    const gaddr row = level0(sDisc->item_table) + item * kItemSize;
    put<std::uint16_t>(row + kItemMaxAmmo, static_cast<std::uint16_t>(std::min(cap, 65535)));

    const gaddr ammo = exe(sDisc->ammo) + item * sizeof(std::int32_t);
    const int current = get<std::int32_t>(ammo);
    if (current > cap) put<std::int32_t>(ammo, cap);
    sAppliedAmmoLevel[static_cast<std::size_t>(weapon)] = level;
}

void apply_all_ammo_caps(bool force = false) {
    for (int i = 0; i < static_cast<int>(weapons().size()); ++i) apply_ammo_capacity(i, force);
}

void refund_survival_ammo(int weapon) {
    if (sAmmoMode != "survival" || weapon < 0 || weapon >= static_cast<int>(weapons().size())) return;
    const auto& def = weapons()[static_cast<std::size_t>(weapon)];
    if (!def.has_ammo || std::string(def.key) == "glove_of_doom") return;
    const int cap = current_ammo_cap(weapon);
    if (cap <= 0) return;
    const gaddr at = exe(sDisc->ammo) + static_cast<std::uint32_t>(def.item_id) * sizeof(std::int32_t);
    const int before = get<std::int32_t>(at);
    if (before < cap) put<std::int32_t>(at, std::min(cap, before + 1));
}

int nanotech_orbs_for(double points, double max_points, int retail_max_orbs) {
    if (points >= max_points) return retail_max_orbs;
    if (points <= 0.0) return 0;
    return std::clamp(
        static_cast<int>(std::ceil(points / std::max(1.0, max_points) * (retail_max_orbs - 1))),
        1,
        std::max(1, retail_max_orbs - 1)
    );
}
