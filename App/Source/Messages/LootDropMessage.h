#pragma once

#include <string>

namespace psr {

// One item dropped on the ground, published by LootDropSystem right after
// the ground entity is spawned. Name is fully resolved (not a prefab id) so
// HudLayer never needs ECS access to show it -- same contract as
// CombatLogEntryMessage.
struct LootDropMessage
{
    std::string item_name;
};

} // namespace psr
