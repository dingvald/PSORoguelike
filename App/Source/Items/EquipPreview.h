#pragma once

#include "Components/StatsComponent.h"

#include <entt/entt.hpp>

#include <optional>

namespace psr {

class Registry;
class AffixLibrary;

// The StatsComponent delta (ComputeEffectiveStats "after" minus "before") from
// hypothetically equipping `item` in place of whatever currently occupies its
// EquipmentSlot -- drives the Character screen's stat-change hover preview
// (see HudLayer's stat-preview members). Computed by temporarily swapping the
// relevant EquipmentComponent field, calling ComputeEffectiveStats, then
// restoring it; safe because EquipmentComponent has no on_update hook (see
// EquipmentComponent.cpp's AttachHandlers, which only binds construct/
// destroy), so a same-call mutate-then-restore is invisible to every other
// system. Returns nullopt if `item` has neither a WeaponComponent nor an
// ArmorComponent (nothing to swap in, via ResolveEquipSlot), or `actor` has
// no EquipmentComponent to swap against.
std::optional<StatsComponent> ComputeEquipStatDelta(Registry& registry, entt::entity actor, entt::entity item,
                                                    const AffixLibrary& affixes);

} // namespace psr
