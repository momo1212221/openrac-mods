#pragma once

#include "progression.hpp"

#include <array>
#include <cstdint>
#include <iosfwd>

namespace arsenal {

constexpr int kProgressFileVersion = 3;

struct PersistedState {
    int source_version = 0;
    bool new_game_plus = false;
    double nanotech_xp = 0.0;
    std::array<WeaponState, kWeaponCount> weapons{};
};

// Writes the current Arsenal Mastery sidecar format. The game save itself is never touched.
bool write_progress(std::ostream& out, const Progression& progression, const NanotechProgression& nanotech);

// Parses format 1 (the first development port) and the current format. Parsing is transactional: on
// failure, `out` is left untouched. Unknown weapon keys are ignored for forward compatibility.
bool read_progress(std::istream& in, PersistedState& out);

// Applies already-validated persisted state while preserving the caller's current settings.
void apply_progress(const PersistedState& state, Progression& progression, NanotechProgression& nanotech);

}  // namespace arsenal
