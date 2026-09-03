#pragma once

#include "Engine/ECS/Entity.h"

namespace psr {

// Binds actor's InventoryComponent::items[inventory_index] into their Item
// hotbar slot hotbar_slot, replacing whatever was previously bound there --
// bound by the item's PrefabIdComponent NameId, same "prefab id, not
// inventory index" style GameplayLayer::TryActivateSlot's Item case already
// resolves at activation time (an index would go stale as the inventory
// reshuffles). A missing InventoryComponent/HotbarComponent, an
// out-of-range inventory_index/hotbar_slot, or an item without
// ConsumableComponent (nothing else resolves through an Item hotbar slot)
// is a no-op. Free/instant, same reasoning as EquipItem/UnequipSlot.
// Returns whether anything changed.
bool AssignItemToHotbarSlot(Entity actor, int inventory_index, int hotbar_slot);

} // namespace psr
