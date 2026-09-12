#pragma once

#include <cstdint>
#include <entt/entt.hpp>

namespace psr {

class Registry;
class Grid;

// Replaces unlocked_door_entity (a plain door prop with no DoorComponent -- see
// DungeonInstantiation::room_entry_doors) with a fresh entity stamped from
// locked_door_prefab_id at the same Position, tagged with a
// DoorComponent{RoomCleared, unlocked_prefab_id, room_index, 0} so DoorUnlock.h's
// UnlockDoor can later restore it. Mirrors UnlockDoor's own destroy-then-stamp sequence in
// reverse. Returns entt::null, doing nothing further, if unlocked_door_entity is no longer
// valid (already locked from the other side of a room-to-room connection) or
// locked_door_prefab_id isn't a registered prefab.
entt::entity LockDoor(Registry& registry, Grid& grid, entt::entity unlocked_door_entity,
                      std::uint32_t locked_door_prefab_id, std::uint32_t unlocked_prefab_id,
                      std::uint32_t room_index);

} // namespace psr
