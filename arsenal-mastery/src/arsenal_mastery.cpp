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
