#pragma once

#include <cstdint>

namespace psr {

// Dispatched to the picking-up entity's own EventHandlerComponent
// (Entity::Dispatch) by PickupAction once an item moves from the Grid into
// its InventoryComponent. CombatLogBridge::OnItemPickup already subscribes to
// this and formats a log line.
struct AfterItemPickupEvent
{
    std::uint32_t item_prefab_id = 0;
};

} // namespace psr
