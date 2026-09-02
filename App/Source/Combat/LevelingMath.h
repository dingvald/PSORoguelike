#pragma once

#include "Combat/LevelingConfig.h"

namespace psr {

// Total lifetime EXP an actor must have accumulated to be at least `level`
// -- LevelComponent::current_exp is compared against this directly, never
// against a "remaining" amount. Pure formula (mirrors CombatMath.h's own
// style): no per-level table to author, just LevelingConfig's two tunable
// constants.
int ExpRequiredForLevel(int level, const LevelingConfig& config);

} // namespace psr
