#pragma once

#include "Engine/ECS/Entity.h"

#include <entt/entt.hpp>

#include <optional>

namespace psr {

class Registry;

// Which EquipmentComponent field an item occupies -- Weapon for a
// WeaponComponent-tagged item, the other four mirroring ArmorComponent's
// ArmorSlot. Not schema-registered: purely an in-memory routing concept for
// EquipItem/UnequipSlot and the Character screen's messages, never authored.
enum class EquipmentSlot
{
    Weapon,
    Head,
    Torso,
    Hands,
    Legs
};

// Which EquipmentSlot `item` would occupy if equipped, from its own
// WeaponComponent/ArmorComponent -- nullopt if it has neither (nothing to
// route it to). Shared by EquipItem below and CharacterScreenSnapshot.cpp,
// which needs the same routing to tag each inventory ItemEntry with whether/
// where it's equippable without duplicating this switch.
std::optional<EquipmentSlot> ResolveEquipSlot(const Registry& registry, entt::entity item);

// Moves inventory->items[inventory_index] into whichever EquipmentComponent
// slot its own WeaponComponent/ArmorComponent implies, swapping whatever
// previously occupied that slot back into the inventory. A missing
// InventoryComponent, an out-of-range index, or an item with neither
// component (nothing to route it to) is a no-op. Free/instant -- no IAction,
// no turn cost, since the Character screen this drives is only reachable
// while the turn loop is already paused (see CharacterScreenState). Returns
// whether anything changed.
bool EquipItem(Entity actor, int inventory_index);

// Moves whatever occupies `slot` back into the actor's InventoryComponent
// (via GetOrEmplace, same as PickupAction) and clears the slot. A no-op if
// the slot is already empty or the inventory is already at capacity (the
// item stays equipped rather than being dropped -- there is no floor here,
// the screen is modal). Returns whether anything changed.
bool UnequipSlot(Entity actor, EquipmentSlot slot);

} // namespace psr
