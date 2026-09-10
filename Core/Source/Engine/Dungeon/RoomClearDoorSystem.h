#pragma once

#include "Engine/Dungeon/PendingSpawnWave.h"
#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace psr {

// Unlocks every RoomCleared-condition door gating a room once every entity ever spawned
// into that room (across all of its waves, not just the current one) has died. Takes the
// same (initial_wave_counts, pending_spawn_waves) DungeonInstantiator handed
// SpawnWaveSystem, summed per group into a single total-remaining counter here -- unlike
// SpawnWaveSystem, this system doesn't care about wave boundaries, only the room's
// eventual total. Reacts to SpawnWaveComponent being destroyed via entt's own on_destroy
// signal, coexisting independently alongside SpawnWaveSystem's own subscription to the
// same signal (mirrors TurnCoordinator/SpawnWaveSystem's shared "react to
// ActorComponent/SpawnWaveComponent destruction rather than a bespoke DeathEvent"
// precedent).
class RoomClearDoorSystem
{
public:
    RoomClearDoorSystem(Registry& registry, Grid& grid,
                        const std::unordered_map<std::uint32_t, int>& initial_wave_counts,
                        const std::vector<PendingSpawnWave>& pending_waves,
                        std::unordered_map<std::uint32_t, std::vector<entt::entity>> room_cleared_doors);
    ~RoomClearDoorSystem();

    // Bound on_destroy<SpawnWaveComponent> listener captures this instance's
    // address -- neither copying nor moving would keep it valid (C.21/C.81).
    RoomClearDoorSystem(const RoomClearDoorSystem&) = delete;
    RoomClearDoorSystem& operator=(const RoomClearDoorSystem&) = delete;
    RoomClearDoorSystem(RoomClearDoorSystem&&) = delete;
    RoomClearDoorSystem& operator=(RoomClearDoorSystem&&) = delete;

private:
    void OnSpawnWaveComponentDestroyed(entt::registry& registry, entt::entity entity);
    void UnlockRoom(std::uint32_t group_id);

    Registry* m_registry;
    Grid* m_grid;
    std::unordered_map<std::uint32_t, int> m_remaining_in_room;
    std::unordered_map<std::uint32_t, std::vector<entt::entity>> m_room_cleared_doors;
};

} // namespace psr
