// SPDX-License-Identifier: MIT
// Copyright (c) 2026 AsobiCodes
// OpenRAC port: Moritz Wallinger
//
// Arsenal Mastery for OpenRAC. This native port keeps its own progression sidecar per save slot and
// uses only OpenRAC's published mod interface plus the same documented RAC1 globals already used by
// OpenRAC's first-party mod examples. It deliberately does not patch machine code.
#include "progression.hpp"
#include "persistence.hpp"
#include "ui_style.hpp"
#include "native_ui.hpp"

#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <mutex>
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

constexpr const char* kModVersion = "0.4.8";
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
    std::uint32_t item_owned;
};

constexpr Disc kDiscs[] = {
    {"pal", 0x0013F450u, 0x001416F8u, 0x0015EEA0u, 0x0013D530u, 0x00140508u,
     0x001404E0u, 0x001C43B0u, 0x00178200u, 0x0015EF20u, 0x0013E620u, 0x0013D5C8u},
    {"ntsc", 0x0013F350u, 0x001415F8u, 0x0015EDA0u, 0x0013D428u, 0x00140408u,
     0x001403E0u, 0x001C40B0u, 0x00178100u, 0x0015EE20u, 0x0013E520u, 0x0013D4C0u},
};

// Native weapons grid draw, exposed on PAL and NTSC by OpenRAC's RAC1 bindings.
struct DrawWeaponGrid {
    static constexpr const char* id = "rac1.menu.item_grid.draw";
    static constexpr const char* name = "Menu_ItemGrid_Draw";
    static constexpr const char* signature = "i32(u32)";
    using fn = std::int32_t (*)(std::uint32_t);
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
bool sRuntimeAnnounced = false;
int sUiPageIndex = 0;
int sSelectedWeapon = 0;
openrac::UiPage sWeaponDetails;
bool sOverlayEnabled = false;
// Grid item order is the original five-column weapons page from bars.luau.
constexpr std::array<int, 15> kOriginalGridItemIds{{
    10, 16, 15, 20, 17,
    14, 9, 11, 18, 13,
    25, 24, 19, 21, 23
}};
std::array<arsenal::native_ui::MenuCell, 15> sCapturedGrid{};
bool sSawOriginalGrid = false;
std::mutex sNativeUiMutex;
arsenal::native_ui::Snapshot sNativeUi;
std::array<arsenal::native_ui::LevelUp, 3> sUiEvents{};
int sUiEventCount = 0;
int sUiHeld = -1;
float sUiSmooth = 0.0f;

// on_frame and the overlay renderer run on different threads: publish a compact atomic snapshot.
std::atomic<int> sHudActive{0}, sHudWeapon{-1}, sHudLevel{1}, sHudPercent{0}, sHudMax{0};
std::atomic<int> sHudNanoLevel{0}, sHudNanoPercent{0}, sHudShowNano{0}, sHudUpgradeTicks{0};
std::array<int, arsenal::kWeaponCount> sHudKnownLevels{};
int sHudUpgradeHold = 0;
int sNanoDisplayHold = 0;
double sNanoPreviousXp = -1.0;
static_assert(arsenal::kWeaponCount == arsenal::ui::kWeaponsPerPage * 2,
              "Update page mapping if the weapon count changes");

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

int current_item() {
    openrac_pose pose{};
    if (sMod != nullptr && sMod->hero_pose(pose) && pose.item >= 0) {
        return pose.item;
    }
    return get<std::int32_t>(exe(sDisc->item_in_hand));
}

// The hook captures only the game's own 5x3 weapons grid. The frame renderer
// must not read game memory; on_frame will publish this capture as a value snapshot.
// Item-grid cells are 0x0A bytes, with kind at +4 and item id at +6.
template <class Original>
std::int32_t on_draw_weapon_grid(Original original, std::uint32_t grid) {
    const auto result = original(grid);
    if (grid == 0 || !sXpDisplay) return result;
    const int rows = get<std::int32_t>(grid + 0x40u);
    const int cols = get<std::int32_t>(grid + 0x44u);
    if (rows != 3 || cols != 5) return result;
    const std::uint32_t cells = get<std::uint32_t>(grid + 0x48u);
    if (cells < 0x100000u || cells >= 0x2000000u) return result;
    std::array<arsenal::native_ui::MenuCell, 15> captured{};
    for (std::size_t i = 0; i < captured.size(); ++i) {
        const auto at = cells + static_cast<std::uint32_t>(i) * 0x0Au;
        if (get<std::int16_t>(at + 4u) != 0) return result;
        const int item = get<std::int16_t>(at + 6u);
        if (item != kOriginalGridItemIds[i]) return result;
        captured[i].weapon = arsenal::weapon_from_item(item);
        if (captured[i].weapon < 0) return result;
        captured[i].owned = get<std::uint8_t>(exe(sDisc->item_owned)
                          + static_cast<std::uint32_t>(item)) != 0;
    }
    sCapturedGrid = captured;
    sSawOriginalGrid = true;
    return result;
}

int current_health(int max_orbs) {
    openrac_pose pose{};
    if (sMod != nullptr && sMod->hero_pose(pose) && pose.health >= 0) {
        return std::clamp(pose.health, 0, max_orbs);
    }
    return std::clamp(get<std::int32_t>(exe(sDisc->hero_health)), 0, max_orbs);
}

void reset_runtime_state() {
    sSawOriginalGrid = false;
    sCapturedGrid = {};
    sUiEventCount = 0;
    sUiHeld = -1;
    sUiSmooth = 0.0f;
    {
        std::lock_guard<std::mutex> guard(sNativeUiMutex);
        sNativeUi = {};
    }
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
    sHudKnownLevels.fill(0);
    sHudUpgradeHold = 0;
    sNanoDisplayHold = 0;
    sNanoPreviousXp = -1.0;
    sHudActive.store(0, std::memory_order_relaxed);
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
        return arsenal::weapon_from_item(current_item());
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

void sync_nanotech() {
    if (!sNanotechEnabled || sMod->loaded_level() < 0) {
        sNanoPool = -1.0;
        sNanoLastOrbs = -1;
        return;
    }

    const int max_orbs = std::clamp(get<std::int32_t>(exe(sDisc->max_health)), 1, 16);
    const int health = current_health(max_orbs);
    const double wanted_max = static_cast<double>(sNanotech.max_points(sProgression.new_game_plus(), max_orbs));

    if (sNanoPool < 0.0 || sNanoLastOrbs < 0) {
        sNanoMax = wanted_max;
        sNanoPool = health >= max_orbs ? sNanoMax : sNanoMax * health / max_orbs;
        sNanoLastOrbs = health;
        sLastNanoLevel = sNanotech.level(sProgression.new_game_plus());
        return;
    }

    if (std::fabs(wanted_max - sNanoMax) > 0.0001) {
        if (wanted_max > sNanoMax) sNanoPool = std::min(wanted_max, sNanoPool + wanted_max - sNanoMax);
        else sNanoPool = std::min(sNanoPool, wanted_max);
        sNanoMax = wanted_max;
    }

    if (health > sNanoLastOrbs) {
        if (health >= max_orbs && sNanoLastOrbs == 0) sNanoPool = sNanoMax;
        else sNanoPool = std::min(sNanoMax, sNanoPool + (health - sNanoLastOrbs) * sNanoMax / max_orbs);
    } else if (health < sNanoLastOrbs) {
        sNanoPool = health <= 0 ? 0.0 : std::min(sNanoPool, sNanoMax * health / max_orbs);
    }
    sNanoLastOrbs = health;

    const int level = sNanotech.level(sProgression.new_game_plus());
    if (level > sLastNanoLevel) {
        sLastNanoLevel = level;
        sMod->info("Nanotech reached N" + std::to_string(level) + " (" + std::to_string(static_cast<int>(sNanoMax)) + " points)");
    }
}

double nanotech_damage_per_hit() {
    const int level = sMod->loaded_level();
    double damage = level >= 0 && level < static_cast<int>(kNanotechDamage.size())
                        ? kNanotechDamage[static_cast<std::size_t>(level)]
                        : kNanotechDamage.back();
    if (sProgression.new_game_plus()) damage *= 2.0;
    if (sRebalanceMode == "hard") damage *= 1.3;
    damage *= sEnemyDamage;
    return damage;
}

void show_level_notice(int weapon, int before, int after) {
    if (after <= before || weapon < 0 || weapon >= static_cast<int>(weapons().size())) return;
    if (sTrace) {
        sMod->info(std::string(weapons()[static_cast<std::size_t>(weapon)].display_name)
                   + " reached V" + std::to_string(after));
    }
    if (!sXpDisplay) return;
    if (sUiEventCount >= static_cast<int>(sUiEvents.size())) {
        std::move(sUiEvents.begin() + 1, sUiEvents.end(), sUiEvents.begin());
        --sUiEventCount;
    }
    sUiEvents[static_cast<std::size_t>(sUiEventCount++)] = {weapon, after, sTick};
}

void award_resolved_score(
    std::uint32_t victim,
    const PendingScore& pending,
    double health_after,
    bool killed_by_hit
) {
    const arsenal::HitScore score = arsenal::score_resolved_hit(
        pending.health_before,
        health_after,
        pending.starting_health,
        pending.effective_damage,
        killed_by_hit
    );

    if (killed_by_hit) sVictims.erase(victim);
    else note_predicted_health(victim, std::max(0.0, health_after));

    if (pending.weapon < 0) {
        if (sTrace && (score.removed > 0.0 || std::fabs(pending.scaled_damage - pending.raw_damage) > 0.0001f)) {
            sMod->info(
                "unattributed enemy hit rebalanced: " + std::to_string(pending.raw_damage)
                + "->" + std::to_string(pending.scaled_damage)
                + " removed=" + std::to_string(score.removed)
            );
        }
        return;
    }

    if (!(score.removed > 0.0) && !score.killed) return;

    const int weapon = pending.weapon;
    const int before = sProgression.level(weapon);
    const int after = sProgression.add_xp(weapon, score.xp);
    if (sNanotechEnabled) sNanotech.add_xp(score.removed, sNanotechXpRate);
    if (score.killed) {
        sProgression.add_kill(weapon);
        refund_survival_ammo(weapon);
    }
    mark_dirty();
    show_level_notice(weapon, before, after);
    if (after != before) apply_ammo_capacity(weapon, true);

    if (sTrace) {
        const auto& state = sProgression.state(weapon);
        sMod->info(
            std::string("resolved hit ") + weapons()[static_cast<std::size_t>(weapon)].key
            + " dmg=" + std::to_string(pending.raw_damage) + "->" + std::to_string(pending.scaled_damage)
            + " removed=" + std::to_string(score.removed)
            + (score.killed ? " KILL" : "")
            + " xp+=" + std::to_string(score.xp * sProgression.settings().xp_rate)
            + " total=" + std::to_string(static_cast<int>(std::lround(state.xp)))
            + " V" + std::to_string(after) + " T=" + std::to_string(pending.balance)
        );
    }
}

void finalize_pending(std::uint32_t victim, bool death) {
    const auto found = sPendingScores.find(victim);
    if (found == sPendingScores.end()) return;
    const PendingScore pending = found->second;
    sPendingScores.erase(found);

    double after = std::max(0.0, pending.health_before - pending.effective_damage);
    if (death) {
        after = 0.0;
    } else if (const std::optional<double> health = enemy_health(victim)) {
        after = *health;
    }

    // Death is credited only when the game's death record happens in the same game tick as this
    // pending hit. An older weapon hit followed by a later hazard therefore cannot steal the kill.
    const bool killed_by_hit = death && pending.tick == sTick;
    award_resolved_score(victim, pending, after, killed_by_hit);
}

void finalize_all_pending() {
    if (sPendingScores.empty()) return;
    std::vector<std::uint32_t> victims;
    victims.reserve(sPendingScores.size());
    for (const auto& entry : sPendingScores) victims.push_back(entry.first);
    for (const std::uint32_t victim : victims) finalize_pending(victim, false);
}

template <class Original>
std::uint32_t on_get_hit(Original original, std::uint32_t victim, std::int32_t mask, std::int32_t keep) {
    const std::uint32_t hit = original(victim, mask, keep);
    if (hit == 0 || !is_enemy(victim)) return hit;

    const std::uint32_t attacker = get<std::uint32_t>(hit + kHitAttacker);
    const int weapon = weapon_from_attacker(attacker);

    const float raw_damage = get<float>(hit + kHitDamage);
    if (!(raw_damage > 0.0f) || repeated_hit(hit, victim, attacker, raw_damage)) return hit;

    // A new distinct hit to this victim means any earlier pending one has already had a chance to
    // affect its health. Finalize that delta before establishing the new pre-hit health.
    finalize_pending(victim, false);

    const double balance = rebalance_factor();
    const double player_power = weapon >= 0 ? weapon_power(weapon) * sWeaponDamage : 1.0;
    const float scaled = static_cast<float>(raw_damage * player_power / std::max(0.01, balance));
    if (std::fabs(scaled - raw_damage) > 0.0001f) put<float>(hit + kHitDamage, scaled);
    remember_hit(hit, victim, attacker, raw_damage, scaled);

    const std::optional<double> health = enemy_health(victim);
    if (!health || !(*health > 0.0)) {
        if (sTrace) {
            sMod->warn("enemy health unavailable: hit rebalanced but no Arsenal Mastery XP was guessed");
        }
        return hit;
    }

    const double start_health = starting_health_for(victim, *health);
    const double predicted_removed = std::min(*health, std::max(0.0, static_cast<double>(scaled)));
    note_predicted_health(victim, std::max(0.0, *health - predicted_removed));
    sPendingScores[victim] = PendingScore{
        weapon,
        *health,
        start_health,
        std::max(0.0, static_cast<double>(scaled)),
        raw_damage,
        scaled,
        balance,
        sTick,
    };
    return hit;
}

template <class Original>
void on_record_death(Original original, std::uint32_t moby, std::int32_t flags, std::int32_t index) {
    // RecordDeath is called while the dying moby still identifies the victim. Finalize before the
    // original clears/saves its death state so the pending weapon hit can receive the kill bonus.
    finalize_pending(moby, true);
    original(moby, flags, index);
}

template <class Original>
void on_take_damage(Original original, std::int32_t amount) {
    if (amount <= 0) {
        original(amount);
        return;
    }

    if (sNanotechEnabled && sMod->loaded_level() >= 0) {
        sync_nanotech();
        if (sNanoPool < 0.0) {
            original(amount);
            return;
        }

        const int max_orbs = std::clamp(get<std::int32_t>(exe(sDisc->max_health)), 1, 16);
        sNanoPool = std::max(0.0, sNanoPool - nanotech_damage_per_hit());
        const int want = nanotech_orbs_for(sNanoPool, sNanoMax, max_orbs);
        put<std::int32_t>(exe(sDisc->hero_health), std::min(want + 1, max_orbs));
        original(1);
        sNanoLastOrbs = want;
        return;
    }

    sEnemyDamageCarry += std::max(0.0, sEnemyDamage);
    int hits = static_cast<int>(std::floor(sEnemyDamageCarry));
    sEnemyDamageCarry -= hits;
    for (int i = 0; i < hits; ++i) original(1);

    if (hits > 0 && sRebalanceMode == "hard") {
        ++sHeroPaidHits;
        if (sHeroPaidHits % 2 == 0) original(1);
    }
}


template <class Original>
std::uint32_t on_load_gameplay(Original original, std::int32_t full) {
    const std::uint32_t cursor = original(full);
    snapshot_retail_ammo_caps();
    apply_all_ammo_caps(true);
    sNanoPool = -1.0;
    sNanoLastOrbs = -1;
    return cursor;
}

template <class Original>
void on_new_game(Original original) {
    finalize_all_pending();
    if (sSaveSlot >= 0 && sDirty) save_progress(sSaveSlot);
    original();
    sProgression.reset();
    sNanotech.reset();
    reset_runtime_state();
    sSaveSlot = -1;
    sFreshNewGame = true;
    mark_dirty();
    sMod->info("New Game: Arsenal Mastery progression reset");
}

template <class Original>
void on_new_game_plus(Original original, std::int32_t slot) {
    finalize_all_pending();
    if (sSaveSlot >= 0 && sDirty) save_progress(sSaveSlot);
    original(slot);
    sProgression.set_new_game_plus(true);
    reset_runtime_state();
    mark_dirty();
    sMod->info("New Game+: Arsenal Mastery progression preserved");
}

void sync_challenge_mode() {
    const bool ng = get<std::int32_t>(exe(sDisc->challenge)) > 0;
    if (ng != sProgression.new_game_plus()) {
        sProgression.set_new_game_plus(ng);
        sNanoPool = -1.0;
        sNanoLastOrbs = -1;
        mark_dirty();
    }
}


openrac::UiContent weapon_list_content() {
    openrac::UiContent content;
    content.title = sUiPageIndex == 0 ? "Weapons 1/2" : "Weapons 2/2";
    const bool in_game = sMod != nullptr && sMod->loaded_level() >= 0;
    for (int row = 0; row < arsenal::ui::kWeaponsPerPage; ++row) {
        const int i = arsenal::ui::weapon_for_row(sUiPageIndex, row);
        const std::string label = std::string(weapons()[static_cast<std::size_t>(i)].display_name)
                                  + "   V" + std::to_string(sProgression.level(i));
        content.items.push_back(openrac::UiItem::button(label, in_game));
    }
    content.items.push_back(openrac::UiItem::button("Nanotech", in_game && sNanotechEnabled));
    content.items.push_back(openrac::UiItem::button(
        sUiPageIndex == 0 ? "More Weapons" : "Previous Weapons"
    ));
    // The real game renders these pages with its own cursor, sounds, font and default footer.
    return content;
}

openrac::UiContent weapon_detail_content() {
    openrac::UiContent content;
    if (sSelectedWeapon < 0) {
        content.title = "Nanotech";
        content.items.push_back(openrac::UiItem::text(
            "Level  N" + std::to_string(sNanotech.level(sProgression.new_game_plus()))
        ));
        content.items.push_back(openrac::UiItem::text(
            "Experience  " + std::to_string(arsenal::ui::percent(
                sNanotech.progress(sProgression.new_game_plus())
            )) + "%"
        ));
        if (sNanoPool >= 0.0) {
            content.items.push_back(openrac::UiItem::text(
                "Health  " + std::to_string(static_cast<int>(std::ceil(sNanoPool))) + "/"
                           + std::to_string(static_cast<int>(std::lround(sNanoMax)))
            ));
        }
        content.items.push_back(openrac::UiItem::text(
            "Total XP  " + std::to_string(static_cast<std::uint64_t>(std::max(0.0, sNanotech.xp())))
        ));
    } else {
        const int i = sSelectedWeapon;
        const auto& def = weapons()[static_cast<std::size_t>(i)];
        const auto& state = sProgression.state(i);
        content.title = def.display_name;
        content.items.push_back(openrac::UiItem::text("Level  V" + std::to_string(sProgression.level(i))));
        content.items.push_back(openrac::UiItem::text(
            "Experience  " + std::to_string(arsenal::ui::percent(sProgression.progress(i))) + "%"
        ));
        content.items.push_back(openrac::UiItem::text(
            "Total XP  " + std::to_string(static_cast<std::uint64_t>(std::max(0.0, state.xp)))
        ));
        content.items.push_back(openrac::UiItem::text(
            "Enemies defeated  " + std::to_string(state.kills)
        ));
        std::ostringstream power;
        power << std::fixed << std::setprecision(2) << sProgression.power(i);
        content.items.push_back(openrac::UiItem::text("Weapon power  x" + power.str()));
        if (def.has_ammo) {
            std::ostringstream ammo;
            ammo << std::fixed << std::setprecision(2)
                 << arsenal::ammo_capacity_factor(sProgression.level(i));
            content.items.push_back(openrac::UiItem::text("Ammo capacity  x" + ammo.str()));
        }
        content.items.push_back(openrac::UiItem::text(
            "Level limit  V" + std::to_string(sProgression.cap(i))
        ));
    }
    content.items.push_back(openrac::UiItem::button("Back to Weapons"));
    return content;
}

void weapon_detail_event(const openrac::UiPage& page, const openrac::UiEvent& event) {
    if (event.is(OPENRAC_UI_CHOOSE)) {
        page.back();
    } else if (event.is(OPENRAC_UI_SHOW) || event.is(OPENRAC_UI_FRAME)) {
        page.set(weapon_detail_content());
    }
}

void progress_page_event(const openrac::UiPage& page, const openrac::UiEvent& event) {
    if (event.is(OPENRAC_UI_CHOOSE)) {
        const int weapon = arsenal::ui::weapon_for_row(sUiPageIndex, event.item());
        if (weapon >= 0 && sMod->loaded_level() >= 0) {
            sSelectedWeapon = weapon;
            sWeaponDetails.set(weapon_detail_content());
            sWeaponDetails.open();
            return;
        }
        if (event.item() == arsenal::ui::kNanotechRow && sMod->loaded_level() >= 0 && sNanotechEnabled) {
            sSelectedWeapon = -1;
            sWeaponDetails.set(weapon_detail_content());
            sWeaponDetails.open();
            return;
        }
        if (event.item() == arsenal::ui::kPageSwitchRow) {
            sUiPageIndex = 1 - sUiPageIndex;
            page.set(weapon_list_content());
            page.set_cursor(0);
            return;
        }
    }
    if (event.is(OPENRAC_UI_SHOW) || event.is(OPENRAC_UI_FRAME)) {
        page.set(weapon_list_content());
    }
}

void update_hud() {
    // Build a coherent frame snapshot without calling game memory from the render thread.
    arsenal::native_ui::Snapshot snapshot{};
    snapshot.enabled = sXpDisplay && sOverlayEnabled;
    snapshot.pal = sDisc != nullptr && std::string(sDisc->version) == "pal";
    snapshot.tick = sTick;
    if (sMod->loaded_level() >= 0) {
        const int phase = sMod->play_status().phase;
        snapshot.playing = phase == OPENRAC_PLAY_PLAYING;
        snapshot.menu_weapons = phase == OPENRAC_PLAY_MENU && sSawOriginalGrid;
        if (snapshot.menu_weapons) {
            snapshot.menu_cells = sCapturedGrid;
            for (auto& cell : snapshot.menu_cells) {
                if (cell.weapon < 0 || !cell.owned) continue;
                cell.level = sProgression.level(cell.weapon);
                cell.progress = static_cast<float>(sProgression.progress(cell.weapon));
            }
            snapshot.wrench_level = sProgression.level(0);
            snapshot.wrench_progress = static_cast<float>(sProgression.progress(0));
        }
        if (snapshot.playing) {
            const int held = arsenal::weapon_from_item(current_item());
            if (held >= 0) {
                const auto& def = weapons()[static_cast<std::size_t>(held)];
                snapshot.held_weapon = held;
                snapshot.level = sProgression.level(held);
                snapshot.progress = static_cast<float>(sProgression.progress(held));
                snapshot.has_ammo = def.has_ammo;
                snapshot.big_ammo = current_ammo_cap(held) >= 100;
                if (held != sUiHeld || snapshot.progress < sUiSmooth) {
                    sUiSmooth = snapshot.progress;
                } else {
                    sUiSmooth += (snapshot.progress - sUiSmooth) * 0.18f;
                }
                sUiHeld = held;
                snapshot.smooth_progress = sUiSmooth;
            } else {
                sUiHeld = -1;
            }
        }
    }
    sSawOriginalGrid = false; // Never reuse a menu capture after it closes.
    for (int i = 0; i < sUiEventCount; ++i) {
        const auto& event = sUiEvents[static_cast<std::size_t>(i)];
        if (sTick < event.tick + 120) {
            snapshot.popups[static_cast<std::size_t>(snapshot.popup_count++)] = event;
        }
    }
    {
        std::lock_guard<std::mutex> guard(sNativeUiMutex);
        sNativeUi = snapshot;
    }
    // Clear the old text HUD if upgrading from a previous build.
    if (sHudVisible && sMod->has_api_12()) {
        sMod->hud_text_set({});
        sHudVisible = false;
    }
}

void draw_native_hud(void*, const openrac_overlay* draw) {
    arsenal::native_ui::Snapshot snapshot;
    {
        std::lock_guard<std::mutex> guard(sNativeUiMutex);
        snapshot = sNativeUi;
    }
    arsenal::native_ui::draw(draw, snapshot);
}

void clean_victim_cache() {
    if (sVictims.size() < 128 && sTick % 300 != 0) return;
    for (auto it = sVictims.begin(); it != sVictims.end();) {
        if (sTick > it->second.tick + kVictimLifetimeTicks) it = sVictims.erase(it);
        else ++it;
    }
}

void on_frame() {
    ++sTick;
    finalize_all_pending();
    sync_save_slot();
    sync_challenge_mode();
    sync_nanotech();
    apply_all_ammo_caps();
    clean_victim_cache();
    if (!sRuntimeAnnounced && sMod->loaded_level() >= 0) {
        sRuntimeAnnounced = true;
        // Keep load diagnostics, but do not show a non-native mod banner in normal gameplay.
        sMod->info("Arsenal Mastery 0.4.8 runtime active");

    }
    update_hud();
    if (sSaveSlot >= 0 && sDirty && sTick >= sLastFlushTick + kFlushTicks) save_progress(sSaveSlot);
}

int start_level_from_setting(const std::string& value) {
    if (value == "off") return 0;
    if (value.size() >= 2 && value[0] == 'v') {
        try {
            const int n = std::stoi(value.substr(1));
            if (n >= 2 && n <= 8) return n;
        } catch (...) {
        }
    }
    return 0;
}

void read_settings(openrac::Mod& mod) {
    Settings settings;
    settings.xp_rate = std::max(0.0, mod.setting<double>("xp_rate"));
    settings.level_power = mod.setting<bool>("level_power");
    settings.gold_at_v5 = mod.setting<bool>("gold_at_v5");
    settings.endless = mod.setting<std::string>("level_cap") == "endless";
    settings.portable_mega = mod.setting<bool>("portable_mega");
    settings.start_level = start_level_from_setting(mod.setting<std::string>("start_level"));
    sProgression.set_settings(settings);

    sRebalanceMode = mod.setting<std::string>("rebalance");
    if (sRebalanceMode != "off" && sRebalanceMode != "standard" && sRebalanceMode != "hard") {
        sRebalanceMode = "standard";
    }
    sAmmoMode = mod.setting<std::string>("ammo_mode");
    if (sAmmoMode != "normal" && sAmmoMode != "survival") sAmmoMode = "normal";
    sNanotechEnabled = mod.setting<bool>("nanotech");
    sXpDisplay = mod.setting<bool>("xp_display");
    sNanotechXpRate = std::max(0.0, mod.setting<double>("nanotech_xp_rate"));
    sWeaponDamage = std::max(0.0, mod.setting<double>("weapon_damage"));
    sEnemyDamage = std::max(0.0, mod.setting<double>("enemy_damage"));
    sTrace = mod.setting<bool>("trace");
}

void find_disc(openrac::Mod& mod) {
    if (mod.game() != "rac1") throw openrac::Error("arsenal-mastery is for Ratchet & Clank (rac1)");
    sDisc = nullptr;
    for (const auto& disc : kDiscs) {
        if (mod.version() == disc.version) {
            sDisc = &disc;
            break;
        }
    }
    if (sDisc == nullptr) {
        throw openrac::Error("arsenal-mastery knows the PAL and NTSC-U discs, not " + std::string(mod.version()));
    }
}

void load(openrac::Mod& mod) {
    sMod = &mod;
    find_disc(mod);
    read_settings(mod);
    sDataDir = mod.data_dir();
    sSaveSlot = -1;
    sFreshNewGame = false;
    sDirty = false;
    sTick = 0;
    sLastFlushTick = 0;
    sRuntimeAnnounced = false;
    sUiEventCount = 0;
    sUiHeld = -1;
    sUiSmooth = 0.0f;
    sNanotech.reset();
    reset_runtime_state();

    mod.hook<DrawWeaponGrid>([](auto original, std::uint32_t grid) {
        return on_draw_weapon_grid(original, grid);
    });
    mod.hook<GetHit>([](auto original, std::uint32_t victim, std::int32_t mask, std::int32_t keep) {
        return on_get_hit(original, victim, mask, keep);
    });
    mod.hook<TakeDamage>([](auto original, std::int32_t amount) { on_take_damage(original, amount); });
    mod.hook<RecordDeath>([](auto original, std::uint32_t moby, std::int32_t flags, std::int32_t index) {
        on_record_death(original, moby, flags, index);
    });
    mod.hook<LoadGameplay>([](auto original, std::int32_t full) { return on_load_gameplay(original, full); });
    mod.hook<NewGame>([](auto original) { on_new_game(original); });
    mod.hook<NewGamePlus>([](auto original, std::int32_t slot) { on_new_game_plus(original, slot); });
    mod.on_frame(on_frame);
    sOverlayEnabled = mod.has_overlay() && mod.on_overlay(draw_native_hud);

    if (mod.has_pause_items()) {
        // Both the menu's shared slots AND its global page pool can be full.
        // Neither failure is fatal for weapon XP, Nanotech or the native HUD.
        try {
            sWeaponDetails = mod.ui_page(weapon_detail_event);
            const openrac::UiPage page = mod.ui_page(progress_page_event);
            try {
                mod.ui_pause_item("Weapons", page);
            } catch (const openrac::Error& error) {
                mod.warn(std::string("In-game Weapons entry unavailable: ") + error.what());
                try {
                    mod.ui_main_item(OPENRAC_UI_OPTIONS, "Weapons", page);
                    mod.info("Weapons page available from the title menu");
                } catch (const openrac::Error& fallback_error) {
                    mod.warn(std::string("Title-menu entry unavailable; gameplay and HUD still work: ")
                             + fallback_error.what());
                }
            }
        } catch (const openrac::Error& error) {
            mod.warn(std::string("Native Weapons pages unavailable; gameplay and HUD still work: ")
                     + error.what());
        }
    }

    mod.info(
        std::string("Arsenal Mastery ") + kModVersion + " loaded on " + std::string(mod.version())
        + "; per-save progression, V1-V99 core, Nanotech and ammo progression enabled"
    );
    if (sProgression.settings().portable_mega) {
        mod.info("Portable Mega XP fallback enabled for weapons whose retail Mega case is not exposed by OpenRAC");
    }
}

}  // namespace

OPENRAC_MOD_CPP("arsenal-mastery", "0.4.8", load)
