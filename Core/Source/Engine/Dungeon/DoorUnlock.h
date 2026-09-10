#pragma once

#include <entt/entt.hpp>

namespace psr {

class Registry;
class Grid;

// Replaces door_entity (a locked door carrying DoorComponent) with a fresh entity
// stamped from its own DoorComponent::unlocked_prefab_id at the same Position -- same
// destroy-then-stamp sequence DungeonInstantiator's own stamp() lambda uses, just split
// across two calls since the old entity must be removed from grid first. Shared by
// RoomClearDoorSystem and SwitchTriggerSystem, the two ways a door can unlock. No-op if
// door_entity is no longer valid, or its unlocked_prefab_id isn't a registered prefab.
void UnlockDoor(Registry& registry, Grid& grid, entt::entity door_entity);

} // namespace psr
