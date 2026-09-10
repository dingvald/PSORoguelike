#pragma once

#include <entt/entt.hpp>

namespace psr {

// Runtime-only tag on a switch entity stamped by DungeonInstantiator at a
// Switch-condition LockAnnotation's switch_cell -- never authored on an entity prefab
// JSON, same convention as SpawnWaveComponent/DoorComponent. target_door is resolved
// once, at instantiation time, to the specific door entity this switch unlocks (which
// may sit in a different placed piece/room than the switch itself). SwitchTriggerSystem
// sets activated on the walk-over that triggers it, so a switch can never double-count
// against DoorComponent::remaining_switches.
struct SwitchComponent
{
    entt::entity target_door = entt::null;
    bool activated = false;
};

} // namespace psr
