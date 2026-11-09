#include "persistence.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <string>

namespace arsenal {
namespace {

bool sane_nonnegative(double value) {
    return std::isfinite(value) && value >= 0.0;
}

bool read_v1(std::istream& in, PersistedState& state) {
    std::string key;
    int ng = 0;
    if (!(in >> key >> ng) || key != "ng-plus") return false;
    state.new_game_plus = ng != 0;

    while (in >> key) {
        double xp = 0.0;
        if (!(in >> xp) || !sane_nonnegative(xp)) return false;
        const int weapon = weapon_from_key(key);
        if (weapon >= 0) state.weapons[static_cast<std::size_t>(weapon)].xp = xp;
    }
    return in.eof();
}

bool read_v3(std::istream& in, PersistedState& state) {
    std::string kind;
    int ng = 0;
    if (!(in >> kind >> ng) || kind != "ng-plus") return false;
    state.new_game_plus = ng != 0;

    if (!(in >> kind >> state.nanotech_xp) || kind != "nanotech-xp"
        || !sane_nonnegative(state.nanotech_xp)) {
        return false;
    }

    while (in >> kind) {
        if (kind != "weapon") return false;
        std::string key;
        double xp = 0.0;
        double mega_xp = -1.0;
        std::uint64_t kills = 0;
        if (!(in >> key >> xp >> mega_xp >> kills)) return false;
        if (!sane_nonnegative(xp) || !std::isfinite(mega_xp) || mega_xp < -1.0) return false;
        if (mega_xp >= 0.0 && mega_xp > xp) return false;

        const int weapon = weapon_from_key(key);
        if (weapon < 0) continue;
        auto& saved = state.weapons[static_cast<std::size_t>(weapon)];
        saved.xp = xp;
        saved.mega_xp = mega_xp;
        saved.kills = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(kills, std::numeric_limits<std::uint32_t>::max())
        );
    }
    return in.eof();
}

}  // namespace

bool write_progress(
    std::ostream& out,
    const Progression& progression,
    const NanotechProgression& nanotech
) {
    out << "arsenal-mastery " << kProgressFileVersion << '\n';
    out << "ng-plus " << (progression.new_game_plus() ? 1 : 0) << '\n';
    out << std::setprecision(17);
    out << "nanotech-xp " << nanotech.xp() << '\n';
    for (std::size_t i = 0; i < weapon_defs().size(); ++i) {
        const auto& state = progression.state(static_cast<int>(i));
        out << "weapon " << weapon_defs()[i].key << ' ' << state.xp << ' ' << state.mega_xp << ' '
            << state.kills << '\n';
    }
    return static_cast<bool>(out);
}

bool read_progress(std::istream& in, PersistedState& out) {
    PersistedState parsed;
    std::string magic;
    int version = 0;
    if (!(in >> magic >> version) || magic != "arsenal-mastery") return false;
    parsed.source_version = version;

    bool ok = false;
    if (version == 1) ok = read_v1(in, parsed);
    if (version == kProgressFileVersion) ok = read_v3(in, parsed);
    if (!ok) return false;

    out = parsed;
    return true;
}

void apply_progress(
    const PersistedState& state,
    Progression& progression,
    NanotechProgression& nanotech
) {
    progression.reset();
    nanotech.reset();
    for (std::size_t i = 0; i < state.weapons.size(); ++i) {
        progression.mutable_state(static_cast<int>(i)) = state.weapons[i];
    }
    nanotech.set_xp(state.nanotech_xp);
    progression.set_new_game_plus(state.new_game_plus);
    progression.refresh();
}

}  // namespace arsenal
