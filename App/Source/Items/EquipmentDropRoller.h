#pragma once

#include <entt/entt.hpp>

#include <cstdint>
#include <random>

namespace psr {

class Registry;

// Rolls drop-time variation onto a freshly-cloned item entity (i.e. right
// after Registry::CreateEntity(item_prefab_id), before it's placed on the
// grid) -- the M8.2 "drop tables" piece WeaponComponent.h/ArmorComponent.h's
// own doc comments call out as not yet built: authored prefab fields
// (StatsComponent, ArmorComponent::mod_slot_count, WeaponComponent::
// grind_level/element/race_bonuses) are templates, this is what turns one
// clone into an actual rolled instance. No-ops for an entity with neither
// an ArmorComponent nor a WeaponComponent (consumables, meseta, mods, ...).
// Pure aside from rng -- same "randomness passed in" shape as
// DropTableRoller::Roll -- except it also needs Registry access to read/
// write the item's own components (DropTableRoller never touches an
// entity).
void RollEquipmentVariation(Registry& registry, entt::entity item, std::mt19937& rng);

} // namespace psr
