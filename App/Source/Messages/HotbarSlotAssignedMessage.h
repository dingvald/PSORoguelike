#pragma once

namespace psr {

// Published by HudLayer once the player, having chosen "Assign to Hotbar"
// from an inventory item's Character-screen context menu, presses a number
// key naming the target slot (already resolved to 0-9 by HudLayer, same
// "fully resolved" contract InventoryItemActivatedMessage/
// EquipmentSlotActivatedMessage use). GameplayLayer subscribes and routes to
// AssignItemToHotbarSlot (Items/Hotbar.h) -- free/instant, same reasoning as
// the Equip case in OnInventoryItemActivated.
struct HotbarSlotAssignedMessage
{
    int inventory_index = -1;
    int hotbar_slot = -1;
};

} // namespace psr
