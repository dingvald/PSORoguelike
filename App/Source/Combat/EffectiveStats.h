#pragma once

#include "Engine/ECS/Entity.h"
#include "Engine/ECS/StatsComponent.h"
#include "Engine/Items/AffixLibrary.h"

namespace psr {

// Sums actor's own StatsComponent with its equipped weapon/armor entities'
// StatsComponent (read via EquipmentComponent -- "stat bonus granted when
// equipped", per WeaponComponent/ArmorComponent's own doc comments) plus any
// flat bonus from the equipped weapon's prefix/suffix affixes. Missing
// components (no StatsComponent on actor, nothing equipped, no matching
// affix) simply contribute nothing, so this is safe to call for any entity.
StatsComponent ComputeEffectiveStats(Entity actor, const AffixLibrary& affixes);

} // namespace psr
