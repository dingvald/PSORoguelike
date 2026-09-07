#pragma once

#include "Engine/ECS/Entity.h"

namespace psr {

// Moves actor's InventoryComponent::items[inventory_index] into its
// StorageComponent. Free/instant, no IAction, same reasoning as
// Items/Equip.h. A no-op if inventory_index is out of range. Always
// succeeds otherwise -- StorageComponent is uncapped. Returns whether
// anything changed.
bool StoreItem(Entity actor, int inventory_index);

// Moves actor's StorageComponent::items[storage_index] back into its
// InventoryComponent. A no-op if storage_index is out of range or the
// inventory is already at capacity (the item stays in storage rather than
// being lost -- there is no floor here, the screen is modal, same
// reasoning UnequipSlot's own capacity guard gives). Returns whether
// anything changed.
bool WithdrawItem(Entity actor, int storage_index);

} // namespace psr
