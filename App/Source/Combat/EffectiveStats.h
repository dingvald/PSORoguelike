#pragma once

#include "Components/StatsComponent.h"
#include "Engine/ECS/Entity.h"
#include "Items/AffixLibrary.h"
#include "Items/Equip.h"

namespace psr {

// Sums actor's own StatsComponent with its equipped weapon/armor entities'
// StatsComponent (read via EquipmentComponent -- "stat bonus granted when
// equipped", per WeaponComponent/ArmorComponent's own doc comments) plus any
// flat bonus from the equipped weapon's prefix/suffix affixes. Missing
// components (no StatsComponent on actor, nothing equipped, no matching
// affix) simply contribute nothing, so this is safe to call for any entity.
StatsComponent ComputeEffectiveStats(Entity actor, const AffixLibrary& affixes);

// Same computation as ComputeEffectiveStats, but with slot's real occupant
// swapped for replacement first -- lets the Character screen preview "what
// if I equipped this instead" (a hovered/keyboard-focused inventory item)
// without actually mutating actor's EquipmentComponent. replacement's own
// affixes apply in place of the real occupant's when slot is Weapon, exactly
// as they would if the swap really happened.
StatsComponent ComputeEffectiveStatsWithSlotOverride(Entity actor, const AffixLibrary& affixes, EquipmentSlot slot,
                                                     entt::entity replacement);

} // namespace psr
