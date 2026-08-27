#pragma once

#include <cstdint>

namespace psr {

// Dispatched to the picking-up entity's own EventHandlerComponent
// (Entity::Dispatch) when an item is picked up. No producer exists anywhere
// in the codebase yet -- there is no inventory/pickup system (see
// EquipmentComponent.h's own doc comment on the same gap). This type exists
// so a listener (e.g. CombatLogBridge) can already subscribe and format a
// log line the moment a future pickup system starts dispatching it; until
// then it is unreachable, not a bug.
struct AfterItemPickupEvent
{
    std::uint32_t item_prefab_id = 0;
};

} // namespace psr
