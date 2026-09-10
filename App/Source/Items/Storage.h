#pragma once

#include "Engine/ECS/Entity.h"

namespace psr {

// Moves actor's InventoryComponent::items[inventory_index] into its
// StorageComponent. Free/instant, no IAction, same reasoning as
// Items/Equip.h. A no-op if inventory_index is out of range. Always
// succeeds otherwise -- StorageComponent is uncapped. If the item is
// stackable (see ItemComponent::max_stack), tops off every existing
// matching storage stack with room first (Storage.cpp's
// MergeIntoMatchingStacks call), then opens a new stack for whatever
// quantity is left over -- unlike Inventory, Storage may hold several full
// stacks of the same item rather than being limited to one slot. Returns
// whether anything changed.
bool StoreItem(Entity actor, int inventory_index);

// Moves actor's StorageComponent::items[storage_index] back into its
// InventoryComponent. A no-op if storage_index is out of range. If a
// matching stack already exists in the inventory (see
// ItemComponent::max_stack), tops it off instead of opening a second slot --
// Inventory is capped at one slot per stackable item type -- leaving any
// quantity that doesn't fit banked; a no-op if that existing stack is
// already full. Otherwise, a no-op if the inventory is already at capacity
// (the item stays in storage rather than being lost -- there is no floor
// here, the screen is modal, same reasoning UnequipSlot's own capacity
// guard gives). Returns whether anything changed.
bool WithdrawItem(Entity actor, int storage_index);

} // namespace psr
