#pragma once

#include "Engine/ECS/Entity.h"

#include <cstdint>

namespace psr {

// Teaches actor technique_id at tier (or, if already known, raises its
// stored tier to max(existing, tier) -- consuming a lower-level disk again
// never downgrades). Always succeeds -- same free/instant, no-failure-case
// shape as EquipItem/UnequipSlot (Items/Equip.h). Called by UseItemAction on
// a ConsumableEffect::TeachTechnique item.
bool LearnTechnique(Entity actor, std::uint32_t technique_id, int tier);

} // namespace psr
