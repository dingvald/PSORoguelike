#pragma once

#include "Engine/ECS/Entity.h"

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

// Which EquipmentSlot item would occupy if equipped -- Weapon for a
// WeaponComponent-tagged item, the matching slot for an ArmorComponent-tagged
// one, nullopt for anything else (a consumable, say) that can't be equipped
// at all. Shared by EquipItem (which actually moves the item) and the
// Character screen's stat-preview (EffectiveStats.h's
// ComputeEffectiveStatsWithSlotOverride), which only needs to know *where*
// a hovered/focused inventory item would go, not to move it.
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
