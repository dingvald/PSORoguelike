#pragma once

#include "Engine/ECS/WeaponComponent.h"

#include <cstdint>
#include <vector>

namespace psr {

// Pure combat-formula helpers, shaped after PSO's known ATA-vs-EVP hit chance
// and ATP-vs-DFP damage mechanics -- not a claimed bit-exact reproduction of
// PSO's original (undocumented) constants. Real per-entity numbers are
// authored content (StatsComponent), same deferral every other milestone
// here already makes; only the formula *shape* is fixed by this file.

// ata/(ata + evp) ratio, clamped to [kMinHitChance, kMaxHitChance] so a hit is
// never guaranteed or impossible.
float ComputeHitChance(int attacker_ata, int defender_evp);

// max(1, round((atp - dfp / 2) * variance_roll)) -- variance_roll is expected
// in [0.9, 1.1] (PSO's small random damage band); a landed hit is never a
// zero.
int ComputeDamage(int attacker_atp, int defender_dfp, float variance_roll);

// Multiplies damage by (1 + bonus_percent / 100) for the race_bonuses entry
// matching defender_race_id, if any; unchanged otherwise.
int ApplyRaceBonus(int damage, const std::vector<RaceBonusEntry>& race_bonuses, std::uint32_t defender_race_id);

} // namespace psr
