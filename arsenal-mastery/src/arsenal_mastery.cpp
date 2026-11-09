// SPDX-License-Identifier: MIT
// Copyright (c) 2026 AsobiCodes
//
// Native OpenRAC port of RAC-RS Arsenal Mastery.
// OpenRAC-specific persistence/HUD integration: Moritz Wallinger.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>

#include "openrac/games/rac1.hpp"
#include "openrac/mod.hpp"

namespace {

struct Weapon {
    const char* name;
    int item;
    double xp_scale;
};

// Item ids and XP scales from RAC-RS Arsenal Mastery.
constexpr std::array<Weapon, 16> kWeapons = {{
    {"OmniWrench", 8, 1.0},
    {"Suck Cannon", 9, 1.0},
    {"Bomb Glove", 10, 1.0},
    {"Devastator", 11, 0.75},
    {"Visibomb Gun", 13, 0.75},
    {"Taunter", 14, 1.0},
    {"Blaster", 15, 1.0},
    {"Pyrocitor", 16, 1.0},
    {"Mine Glove", 17, 0.75},
    {"Walloper", 18, 0.75},
    {"Tesla Claw", 19, 0.75},
    {"Glove of Doom", 20, 0.5},
    {"Morph-o-Ray", 21, 0.5},
    {"R.Y.N.O.", 23, 0.5},
    {"Drone Device", 24, 0.5},
    {"Decoy Glove", 25, 0.5},
}};

struct Attacker {
    int moby_class;
    int weapon;
};

// Projectile/held-object attribution from RAC-RS power.luau. Direct hero hits are resolved through
// OpenRAC's figure slot + hero_pose instead of hard-coded hero globals.
constexpr std::array<Attacker, 27> kAttackers = {{
    {121, 2}, {192, 2},
    {74, 8}, {190, 8},
    {305, 6}, {168, 6},
    {457, 13}, {454, 13},
    {186, 11}, {230, 11}, {229, 11},
    {172, 4}, {163, 4},
    {153, 3}, {157, 3},
    {176, 7}, {177, 10}, {180, 9},
    {175, 5}, {185, 12}, {562, 15}, {849, 1},
    {483, 14}, {479, 14}, {203, 15}, {1900, 15}, {71, 0},
}};

constexpr std::array<double, 4> kXpThresholds = {{750.0, 2250.0, 5000.0, 10000.0}};
constexpr std::array<double, 5> kPower = {{1.0, 1.2, 1.5, 1.9, 2.5}};
constexpr std::array<double, 19> kRebalance = {{
    1.0, 1.0, 1.1, 1.2, 1.3, 1.45, 1.6, 1.75, 1.9, 2.0,
    2.1, 2.2, 2.3, 2.35, 2.4, 2.45, 2.5, 2.5, 2.5
}};
constexpr double kHardFactor = 1.3;
constexpr double kNgPlusFactor = 2.5;
constexpr double kXpPerDamage = 10.0;
constexpr int kSaveSlots = 16;
constexpr int kUnsavedSlot = kSaveSlots;
constexpr int kProgressSlots = kSaveSlots + 1;

openrac::Mod* sMod = nullptr;
std::array<std::array<double, kWeapons.size()>, kProgressSlots> sXp{};
std::uint64_t sTick = 0;
bool sReplayingScaledHit = false;
std::uint64_t sSaveTick = 0;
int sActiveSlot = kUnsavedSlot;
bool sUnsavedSession = false;
bool sDirty = false;
bool sNgPlus = false;
double sXpRate = 1.0;
double sWeaponDamage = 1.0;
bool sLevelPower = true;
bool sEnemyRebalance = true;
bool sHardRebalance = false;
bool sTrace = false;
bool sShowHud = true;
bool sLevelNotice = true;
int sLevelNoticeTicks = 0;
int sLevelNoticeWeapon = -1;
std::string sProgressPath;

std::uint32_t rgba(unsigned r, unsigned g, unsigned b, unsigned a) {
    return ((r & 0xffu) << 24u) | ((g & 0xffu) << 16u) | ((b & 0xffu) << 8u) | (a & 0xffu);
}

int weapon_from_item(int item) {
    for (std::size_t i = 0; i < kWeapons.size(); ++i) {
        if (kWeapons[i].item == item) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int current_item() {
    if (sMod == nullptr) {
        return -1;
    }
    openrac_pose pose{};
    return sMod->hero_pose(pose) ? pose.item : -1;
}

int current_weapon() {
    return weapon_from_item(current_item());
}

bool loaded_mod(const char* id) {
    if (sMod == nullptr) {
        return false;
    }
    for (const auto& entry : sMod->mods()) {
        if (entry.id == id) {
            return true;
        }
    }
    return false;
}


int weapon_from_oclass(int oclass) {
    for (const auto& entry : kAttackers) {
        if (entry.moby_class == oclass) {
            return entry.weapon;
        }
    }
    return -1;
}

int moby_oclass_from_slot(int slot) {
    if (sMod == nullptr || slot < 0) {
        return -1;
    }
    for (const auto& moby : sMod->moby_list()) {
        if (moby.slot == slot) {
            return moby.oclass;
        }
    }
    return -1;
}

int weapon_from_hit(const openrac_hit& hit) {
    if (sMod == nullptr) {
        return -1;
    }

    const int hero_slot = sMod->figure_slot(-1);
    if (hit.attacker >= 0 && hit.attacker == hero_slot) {
        return current_weapon();
    }

    // SDK 1.14.2 exposes the attacker slot but not the later source_oclass extension. For
    // projectiles and weapon-owned mobys the attacker slot's class is enough to use the original
    // Arsenal Mastery attribution table.
    return weapon_from_oclass(moby_oclass_from_slot(hit.attacker));
}

double& weapon_xp(int weapon) {
    return sXp[static_cast<std::size_t>(sActiveSlot)][static_cast<std::size_t>(weapon)];
}

double xp_threshold(int weapon, int next_level) {
    if (weapon < 0 || weapon >= static_cast<int>(kWeapons.size()) || next_level < 2 || next_level > 5) {
        return 0;
    }
    return kXpThresholds[static_cast<std::size_t>(next_level - 2)]
           * kWeapons[static_cast<std::size_t>(weapon)].xp_scale;
}

int weapon_level(int weapon) {
    if (weapon < 0 || weapon >= static_cast<int>(kWeapons.size())) {
        return 1;
    }
    const double xp = weapon_xp(weapon);
    int level = 1;
    for (int next = 2; next <= 5; ++next) {
        if (xp >= xp_threshold(weapon, next)) {
            level = next;
        } else {
            break;
        }
    }
    return level;
}

void load_progress() {
    if (sProgressPath.empty()) {
        return;
    }
    std::ifstream file(sProgressPath);
    std::string header;
    if (!file || !std::getline(file, header) || header != "OPENRAC_ARSENAL_MASTERY_V2") {
        return;
    }

    int slot = 0;
    int weapon = 0;
    double xp = 0;
    while (file >> slot >> weapon >> xp) {
        if (slot >= 0 && slot < kSaveSlots && weapon >= 0
            && weapon < static_cast<int>(kWeapons.size()) && std::isfinite(xp) && xp >= 0) {
            sXp[static_cast<std::size_t>(slot)][static_cast<std::size_t>(weapon)] = xp;
        }
    }
}

void save_progress() {
    if (!sDirty || sProgressPath.empty()) {
        return;
    }
    const std::string temp = sProgressPath + ".tmp";
    std::ofstream file(temp, std::ios::trunc);
    if (!file) {
        return;
    }
    file << "OPENRAC_ARSENAL_MASTERY_V2\n";
    for (int slot = 0; slot < kSaveSlots; ++slot) {
        for (std::size_t weapon = 0; weapon < kWeapons.size(); ++weapon) {
            const double xp = sXp[static_cast<std::size_t>(slot)][weapon];
            if (xp > 0) {
                file << slot << ' ' << weapon << ' ' << xp << '\n';
            }
        }
    }
    file.close();
    if (!file) {
        return;
    }
    std::remove(sProgressPath.c_str());
    if (std::rename(temp.c_str(), sProgressPath.c_str()) == 0) {
        sDirty = false;
    }
}

void update_progress_slot() {
    if (sMod == nullptr || !sMod->has_api_11()) {
        return;
    }

    const auto status = sMod->play_status();
    if (status.phase == OPENRAC_PLAY_FRONT_END) {
        if (sActiveSlot != kUnsavedSlot) {
            save_progress();
        }
        sActiveSlot = kUnsavedSlot;
        sUnsavedSession = false;
        sXp[static_cast<std::size_t>(kUnsavedSlot)].fill(0);
        return;
    }

    const int slot = sMod->save_slot_current();
    if (slot >= 0 && slot < kSaveSlots && slot != sActiveSlot) {
        if (sActiveSlot == kUnsavedSlot && sUnsavedSession) {
            sXp[static_cast<std::size_t>(slot)] = sXp[static_cast<std::size_t>(kUnsavedSlot)];
            sXp[static_cast<std::size_t>(kUnsavedSlot)].fill(0);
            sDirty = true;
        }
        sActiveSlot = slot;
        sUnsavedSession = false;
    } else if (slot < 0 && sActiveSlot == kUnsavedSlot
               && (status.phase == OPENRAC_PLAY_PLAYING || status.phase == OPENRAC_PLAY_SCENE
                   || status.phase == OPENRAC_PLAY_MENU)) {
        sUnsavedSession = true;
    }
}

double rebalance_factor() {
    if (!sEnemyRebalance || sMod == nullptr) {
        return 1.0;
    }
    const int level = sMod->loaded_level();
    double factor = 1.0;
    if (level >= 0 && level < static_cast<int>(kRebalance.size())) {
        factor = kRebalance[static_cast<std::size_t>(level)];
    }
    if (sHardRebalance) {
        factor *= kHardFactor;
    }
    if (sNgPlus) {
        factor *= kNgPlusFactor;
    }
    return factor;
}

int on_moby_hit(void*, const openrac_hit* source) {
    if (sMod == nullptr || source == nullptr || sReplayingScaledHit) {
        return 0;
    }

    const int hero_slot = sMod->figure_slot(-1);
    if (source->victim < 0 || source->victim == hero_slot || !(source->damage > 0)
        || !std::isfinite(source->damage)) {
        return 0;
    }

    const int weapon = weapon_from_hit(*source);
    if (weapon < 0) {
        return 0;
    }

    const int before = weapon_level(weapon);
    const double power = sLevelPower ? kPower[static_cast<std::size_t>(before - 1)] : 1.0;
    const double balance = rebalance_factor();
    const double multiplier = power * sWeaponDamage / std::max(0.01, balance);
    const float wanted_damage = static_cast<float>(source->damage * multiplier);
    float applied_damage = source->damage;
    bool replaced = false;
    const bool multiplayer = loaded_mod("multiplayer");

    // Multiplayer owns hit authority/network forwarding. Keep Arsenal progression visible and
    // persistent there, but do not create a second scaled network hit. Solo play keeps the full
    // Arsenal damage curve and replacement path.
    if (!multiplayer && std::fabs(wanted_damage - source->damage) > 0.0001f) {
        openrac_hit replay = *source;
        replay.size = sizeof replay;
        replay.damage = wanted_damage;

        // on_moby_hit also sees hits posted by mods. The guard lets the replay through unchanged
        // and prevents recursion; the original retail hit is taken back only when the replacement
        // was accepted by OpenRAC.
        sReplayingScaledHit = true;
        replaced = sMod->moby_hit(replay);
        sReplayingScaledHit = false;
        if (replaced) {
            applied_damage = wanted_damage;
        }
    }

    const double gain = static_cast<double>(applied_damage) * kXpPerDamage * sXpRate;
    weapon_xp(weapon) += gain;
    sDirty = true;

    const int after = weapon_level(weapon);
    if (after > before) {
        sLevelNoticeWeapon = weapon;
        sLevelNoticeTicks = sLevelNotice ? 240 : 0;
        save_progress();
        sMod->info(
            std::string(kWeapons[static_cast<std::size_t>(weapon)].name) + " reached V"
            + std::to_string(after)
        );
    }

    if (sTrace) {
        sMod->info(
            std::string("hit ") + kWeapons[static_cast<std::size_t>(weapon)].name
            + " xp+" + std::to_string(static_cast<int>(std::lround(gain)))
            + " total=" + std::to_string(static_cast<int>(std::lround(weapon_xp(weapon))))
            + " V" + std::to_string(after) + " damage=" + std::to_string(source->damage) + "->"
            + std::to_string(applied_damage) + " T=" + std::to_string(balance)
            + (replaced ? " replay=scaled" : multiplayer ? " replay=multiplayer-authority"
                                                     : " replay=original")
        );
    }

    return replaced ? 1 : 0;
}

template <class Original>
void on_new_game(Original original) {
    original();
    sActiveSlot = kUnsavedSlot;
    sUnsavedSession = true;
    sXp[static_cast<std::size_t>(kUnsavedSlot)].fill(0);
    sNgPlus = false;
    sLevelNoticeTicks = 0;
    if (sMod != nullptr) {
        sMod->info("new game: Arsenal Mastery progression reset for the new unsaved game");
    }
}

template <class Original>
void on_new_game_plus(Original original, std::int32_t slot) {
    original(slot);
    sNgPlus = true;
    if (sMod != nullptr) {
        sMod->info("New Game+: Arsenal Mastery progression preserved");
    }
}

void on_frame() {
    ++sTick;
    update_progress_slot();
    if (sLevelNoticeTicks > 0) {
        --sLevelNoticeTicks;
    }
    if (++sSaveTick >= 300) {
        sSaveTick = 0;
        save_progress();
    }
}

void draw_text(
    const openrac_overlay* overlay, float x, float y, float height, std::uint32_t colour,
    const std::string& text
) {
    if (overlay == nullptr || overlay->text == nullptr) {
        return;
    }
    overlay->text(overlay->context, x + 1.2f, y + 1.2f, height, rgba(0, 0, 0, 190), text.c_str());
    overlay->text(overlay->context, x, y, height, colour, text.c_str());
}

void draw_centred(
    const openrac_overlay* overlay, float centre, float y, float height, std::uint32_t colour,
    const std::string& text
) {
    const float width = overlay != nullptr && overlay->text_width != nullptr
                            ? overlay->text_width(overlay->context, height, text.c_str())
                            : 0;
    draw_text(overlay, centre - width * 0.5f, y, height, colour, text);
}

void overlay_frame(void*, const openrac_overlay* overlay) {
    if (sMod == nullptr || overlay == nullptr || overlay->rect == nullptr) {
        return;
    }

    const int weapon = current_weapon();
    if (sShowHud && weapon >= 0) {
        const int level = weapon_level(weapon);
        const double xp = weapon_xp(weapon);
        const float x = 18, y = 362, width = 224, height = 68;
        overlay->rect(overlay->context, x + 3, y + 3, width, height, rgba(0, 0, 0, 140));
        overlay->rect(overlay->context, x, y, width, height, rgba(91, 55, 10, 238));
        overlay->rect(overlay->context, x + 2, y + 2, width - 4, height - 4, rgba(22, 18, 12, 240));
        overlay->rect(overlay->context, x + 4, y + 4, 4, height - 8, rgba(235, 157, 34, 255));

        draw_text(
            overlay, x + 15, y + 9, 8, rgba(255, 232, 168, 255),
            kWeapons[static_cast<std::size_t>(weapon)].name
        );
        draw_text(
            overlay, x + 172, y + 9, 8, rgba(255, 206, 92, 255),
            std::string("V") + std::to_string(level)
        );

        std::string xp_line;
        float progress = 1;
        if (level < 5) {
            const double start = level == 1 ? 0 : xp_threshold(weapon, level);
            const double next = xp_threshold(weapon, level + 1);
            progress = static_cast<float>(std::clamp((xp - start) / std::max(1.0, next - start), 0.0, 1.0));
            xp_line = "WEAPON XP  " + std::to_string(static_cast<int>(std::lround(xp))) + " / "
                      + std::to_string(static_cast<int>(std::lround(next)));
        } else {
            xp_line = "WEAPON XP  " + std::to_string(static_cast<int>(std::lround(xp))) + "  MAX";
        }
        draw_text(overlay, x + 15, y + 29, 5.5f, rgba(226, 211, 180, 240), xp_line);

        const float bar_x = x + 15, bar_y = y + 43, bar_w = width - 30;
        overlay->rect(overlay->context, bar_x, bar_y, bar_w, 8, rgba(8, 7, 5, 220));
        overlay->rect(overlay->context, bar_x + 2, bar_y + 2, (bar_w - 4) * progress, 4,
                      rgba(231, 145, 25, 255));
        for (int i = 0; i < 5; ++i) {
            overlay->rect(
                overlay->context, bar_x + i * 37.5f, y + 56, 32, 5,
                i < level ? rgba(220, 132, 22, 250) : rgba(46, 38, 28, 210)
            );
        }
    }

    if (sLevelNotice && sLevelNoticeTicks > 0 && sLevelNoticeWeapon >= 0) {
        const int weapon_index = sLevelNoticeWeapon;
        const int level = weapon_level(weapon_index);
        const float fade = sLevelNoticeTicks < 35 ? sLevelNoticeTicks / 35.0f : 1.0f;
        const unsigned alpha = static_cast<unsigned>(std::clamp(fade, 0.0f, 1.0f) * 255);
        overlay->rect(overlay->context, 116, 70, 280, 52, rgba(91, 55, 10, alpha));
        overlay->rect(overlay->context, 119, 73, 274, 46, rgba(22, 18, 12, alpha));
        draw_centred(overlay, 256, 82, 7, rgba(255, 224, 142, alpha), "ARSENAL MASTERY");
        draw_centred(
            overlay, 256, 98, 9, rgba(255, 246, 211, alpha),
            std::string(kWeapons[static_cast<std::size_t>(weapon_index)].name) + "  V"
                + std::to_string(level)
        );
    }
}

void load(openrac::Mod& mod) {
    sMod = &mod;
    if (mod.game() != "rac1") {
        throw openrac::Error("arsenal-mastery is for Ratchet & Clank (rac1)");
    }
    if (!mod.has_api_11() || !mod.has_api_12() || !mod.has_overlay()) {
        throw openrac::Error("Arsenal Mastery 0.2.0 requires OpenRAC mod interface 1.14");
    }

    sXpRate = std::max(0.0, mod.setting<double>("xp_rate"));
    sWeaponDamage = std::max(0.0, mod.setting<double>("weapon_damage"));
    sLevelPower = mod.setting<bool>("level_power");
    sEnemyRebalance = mod.setting<bool>("enemy_rebalance");
    sHardRebalance = mod.setting<bool>("hard_rebalance");
    sTrace = mod.setting<bool>("trace");
    sShowHud = mod.setting<bool>("show_hud");
    sLevelNotice = mod.setting<bool>("level_up_notice");

    const std::string dir = mod.data_dir();
    if (!dir.empty()) {
        sProgressPath = dir + "/arsenal-mastery-xp-v2.dat";
        load_progress();
    }
    update_progress_slot();

    // Use the stable hit watcher rather than taking Moby_GetHitMessage. This lets Arsenal
    // Mastery coexist with Enhanced Weapons and other mods that own the retail hook point.
    mod.on_moby_hit(on_moby_hit);
    mod.hook<openrac::rac1::Save_NewGame>([](auto original) { on_new_game(original); });
    mod.hook<openrac::rac1::Save_NewGamePlus>(
        [](auto original, std::int32_t slot) { on_new_game_plus(original, slot); }
    );
    mod.on_frame(on_frame);
    if (!mod.on_overlay(overlay_frame)) {
        throw openrac::Error("Arsenal Mastery could not register its Enhanced RAC HUD");
    }

    mod.info(
        "Arsenal Mastery 0.2.0 loaded: persistent V1-V5 progression + Enhanced RAC HUD enabled"
    );
}

}  // namespace

OPENRAC_MOD_CPP("arsenal-mastery", "0.2.0", load)
