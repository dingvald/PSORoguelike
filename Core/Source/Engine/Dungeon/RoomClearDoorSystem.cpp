#include "Engine/Dungeon/RoomClearDoorSystem.h"

#include "Engine/Dungeon/DoorUnlock.h"
#include "Engine/ECS/SpawnWaveComponent.h"

#include <utility>

namespace psr {

RoomClearDoorSystem::RoomClearDoorSystem(Registry& registry, Grid& grid,
                                         const std::unordered_map<std::uint32_t, int>& initial_wave_counts,
                                         const std::vector<PendingSpawnWave>& pending_waves,
                                         std::unordered_map<std::uint32_t, std::vector<entt::entity>> room_cleared_doors)
    : m_registry(&registry), m_grid(&grid), m_room_cleared_doors(std::move(room_cleared_doors))
{
    for (const auto& [group_id, doors] : m_room_cleared_doors)
    {
        int total = 0;
        if (auto it = initial_wave_counts.find(group_id); it != initial_wave_counts.end())
            total += it->second;
        for (const PendingSpawnWave& wave : pending_waves)
            if (wave.group_id == group_id)
                total += static_cast<int>(wave.entries.size());
        m_remaining_in_room[group_id] = total;
    }

    registry.OnDestroy<SpawnWaveComponent, &RoomClearDoorSystem::OnSpawnWaveComponentDestroyed>(*this);

    // A room authored with a RoomCleared lock but no spawns at all is
    // trivially already cleared -- unlock its doors immediately rather than
    // waiting for a death that will never come. Collect group ids first: this
    // loop must not touch m_room_cleared_doors mid-erase inside UnlockRoom.
    std::vector<std::uint32_t> trivially_cleared;
    for (const auto& [group_id, remaining] : m_remaining_in_room)
        if (remaining <= 0)
            trivially_cleared.push_back(group_id);
    for (std::uint32_t group_id : trivially_cleared)
        UnlockRoom(group_id);
}

RoomClearDoorSystem::~RoomClearDoorSystem() { m_registry->DisconnectComponentLifecycle<SpawnWaveComponent>(*this); }

void RoomClearDoorSystem::OnSpawnWaveComponentDestroyed(entt::registry& registry, entt::entity entity)
{
    const std::uint32_t group_id = registry.get<SpawnWaveComponent>(entity).group_id;

    auto it = m_remaining_in_room.find(group_id);
    if (it == m_remaining_in_room.end())
        return;
    if (--it->second > 0)
        return;

    UnlockRoom(group_id);
}

void RoomClearDoorSystem::UnlockRoom(std::uint32_t group_id)
{
    m_remaining_in_room.erase(group_id);

    auto doors_it = m_room_cleared_doors.find(group_id);
    if (doors_it == m_room_cleared_doors.end())
        return;
    for (entt::entity door : doors_it->second)
        UnlockDoor(*m_registry, *m_grid, door);
    m_room_cleared_doors.erase(doors_it);
}

} // namespace psr
