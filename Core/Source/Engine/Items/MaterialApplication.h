#pragma once

#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/StatsComponent.h"
#include "Engine/Items/MaterialStat.h"

namespace psr {

// Permanently applies one material's effect (see MaterialComponent) to
// stats/health -- amount is added to the matching StatsComponent field, or
// to *both* current_hp and max_hp for MaterialStat::MaxHp (a genuine
// permanent stat increase, not a heal layered on top of an existing wound
// -- current_hp rises by the same amount so the character isn't left
// artificially damaged relative to their new max). A negative amount
// lowers the stat the same way (not clamped here -- authored materials are
// expected to use positive amounts per the GDD's "boost" framing; this
// function stays a pure, unconditional add either way).
void ApplyMaterial(StatsComponent& stats, HealthComponent& health, MaterialStat stat, int amount);

} // namespace psr
