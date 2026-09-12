#pragma once

#include "Engine/Dungeon/PendingSpawnWave.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Math/Vec2.h"
#include "Engine/World/Grid.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace psr {

// Unlocks every RoomCleared-condition door gating a room once every entity ever spawned
// into that room (across all of its waves, not just the current one) has died. Takes the
// same pending_spawn_waves DungeonInstantiator handed SpawnWaveSystem, summed per group
// into a single total-remaining counter here -- unlike SpawnWaveSystem, this system
// doesn't care about wave boundaries, only the room's eventual total. Reacts to
// SpawnWaveComponent being destroyed via entt's own on_destroy signal, coexisting
// independently alongside SpawnWaveSystem's own subscription to the same signal (mirrors
// TurnCoordinator/SpawnWaveSystem's shared "react to ActorComponent/SpawnWaveComponent
// destruction rather than a bespoke DeathEvent" precedent).
//
// Also owns turning a room's plain, currently-open entry doors (room_entry_doors) into
// locked ones the moment the player first steps inside (see LockRoomOnEntry) -- this is
// what makes RoomCleared gating actually enterable at all: DungeonStitcher no longer
// pre-locks these at generation time (see LockAnnotation's own doc comment), so a room
// with spawns starts open and seals behind the player instead.
class RoomClearDoorSystem
{
public:
    RoomClearDoorSystem(Registry& registry, Grid& grid, const std::vector<PendingSpawnWave>& pending_waves,
                        std::unordered_map<std::uint32_t, std::vector<entt::entity>> room_cleared_doors,
                        std::unordered_map<std::uint32_t, std::vector<entt::entity>> room_entry_doors,
                        std::uint32_t locked_door_prefab_id, std::uint32_t unlocked_door_prefab_id);
    ~RoomClearDoorSystem();

    // Bound on_destroy<SpawnWaveComponent> listener captures this instance's
    // address -- neither copying nor moving would keep it valid (C.21/C.81).
    RoomClearDoorSystem(const RoomClearDoorSystem&) = delete;
    RoomClearDoorSystem& operator=(const RoomClearDoorSystem&) = delete;
    RoomClearDoorSystem(RoomClearDoorSystem&&) = delete;
    RoomClearDoorSystem& operator=(RoomClearDoorSystem&&) = delete;

    // Locks every one of group_id's entry doors the first time the player steps into that
    // room, provided it still has at least one spawn pending or alive (m_remaining_in_room)
    // -- a room with no authored spawns, or one already fully cleared, is left untouched.
    // Safe to call again on re-entry: the group's entry-door list is erased once locked, so
    // a later call for an already-locked-and-uncleared room, or an already-cleared one, is
    // a no-op either way. Called once per group the first time the player enters that room
    // -- see GameplayLayer::EnterRoom.
    void LockRoomOnEntry(std::uint32_t group_id);

    // True while `tile` is exactly one of group_id's still-unlocked entry
    // doors -- lets the caller (GameplayLayer::EnterRoom) hold off on
    // LockRoomOnEntry until the player has stepped one tile past the
    // doorway, instead of sealing it in on the same tile the player is
    // still standing on. Always false once the room's already been locked
    // (its entry-door list erased by LockRoomOnEntry).
    bool IsEntryThresholdTile(std::uint32_t group_id, Vec2 tile) const;

private:
    void OnSpawnWaveComponentDestroyed(entt::registry& registry, entt::entity entity);
    void UnlockRoom(std::uint32_t group_id);

    Registry* m_registry;
    Grid* m_grid;
    std::unordered_map<std::uint32_t, int> m_remaining_in_room;
    std::unordered_map<std::uint32_t, std::vector<entt::entity>> m_room_cleared_doors;
    std::unordered_map<std::uint32_t, std::vector<entt::entity>> m_room_entry_doors;
    std::uint32_t m_locked_door_prefab_id;
    std::uint32_t m_unlocked_door_prefab_id;
};

} // namespace psr
