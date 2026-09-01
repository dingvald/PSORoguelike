#pragma once

#include <cstdint>

namespace psr {

// Dispatched to the dropping entity's own EventHandlerComponent (see
// Entity::Dispatch) by DropAction once an item leaves its InventoryComponent
// and lands back on the Grid -- sibling of AfterItemPickupEvent
// (ItemPickupEvent.h), same shape.
struct AfterItemDropEvent
{
    std::uint32_t item_prefab_id = 0;
};

} // namespace psr
