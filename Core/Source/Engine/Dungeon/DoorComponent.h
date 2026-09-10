#pragma once

#include "Engine/Dungeon/DoorUnlockCondition.h"

#include <cstdint>

namespace psr {

// Runtime-only tag on a locked door entity stamped by DungeonInstantiator from a
// LockAnnotation -- never authored on an entity prefab JSON, never passed through
// ComponentSchemaRegistrar, same convention as SpawnWaveComponent. condition ==
// RoomCleared is driven by RoomClearDoorSystem watching room_index's spawned enemies;
// condition == Switch is driven by SwitchTriggerSystem decrementing remaining_switches
// as its (always exactly one, today) switch is triggered. Either system calls
// DoorUnlock.h's UnlockDoor once its condition is met, replacing this entity with one
// stamped from unlocked_prefab_id at the same tile.
struct DoorComponent
{
    DoorUnlockCondition condition = DoorUnlockCondition::RoomCleared;
    std::uint32_t unlocked_prefab_id = 0;
    std::uint32_t room_index = 0;
    int remaining_switches = 0;
};

} // namespace psr
