#pragma once

#include <cstdint>

namespace psr {

// Dispatched to the using entity's own EventHandlerComponent
// (Entity::Dispatch) by UseItemAction once a consumable item's effect has
// been applied and the item entity destroyed. CombatLogBridge::OnItemUse
// subscribes to this and formats a log line.
struct AfterItemUseEvent
{
    std::uint32_t item_prefab_id = 0;
};

} // namespace psr
