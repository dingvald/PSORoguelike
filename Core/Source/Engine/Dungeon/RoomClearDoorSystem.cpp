#include "Engine/Dungeon/RoomClearDoorSystem.h"

#include "Engine/Dungeon/DoorLock.h"
#include "Engine/Dungeon/DoorUnlock.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/SpawnWaveComponent.h"

#include <utility>

namespace psr {

RoomClearDoorSystem::RoomClearDoorSystem(Registry& registry, Grid& grid,
                                         const std::vector<PendingSpawnWave>& pending_waves,
                                         std::unordered_map<std::uint32_t, std::vector<entt::entity>> room_cleared_doors,
                                         std::unordered_map<std::uint32_t, std::vector<entt::entity>> room_entry_doors,
                                         std::uint32_t locked_door_prefab_id, std::uint32_t unlocked_door_prefab_id)
    : m_registry(&registry), m_grid(&grid), m_room_cleared_doors(std::move(room_cleared_doors)),
      m_room_entry_doors(std::move(room_entry_doors)), m_locked_door_prefab_id(locked_door_prefab_id),
      m_unlocked_door_prefab_id(unlocked_door_prefab_id)
{
    for (const PendingSpawnWave& wave : pending_waves)
        m_remaining_in_room[wave.group_id] += static_cast<int>(wave.entries.size());

    registry.OnDestroy<SpawnWaveComponent, &RoomClearDoorSystem::OnSpawnWaveComponentDestroyed>(*this);

    // A room authored with a RoomCleared lock but no spawns at all is
    // trivially already cleared -- unlock its doors immediately rather than
    // waiting for a death that will never come. Collect group ids first: this
    // loop must not touch m_room_cleared_doors mid-erase inside UnlockRoom.
    std::vector<std::uint32_t> trivially_cleared;
    for (const auto& [group_id, doors] : m_room_cleared_doors)
    {
        auto it = m_remaining_in_room.find(group_id);
        if (it == m_remaining_in_room.end() || it->second <= 0)
            trivially_cleared.push_back(group_id);
    }
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

void RoomClearDoorSystem::LockRoomOnEntry(std::uint32_t group_id)
{
    auto remaining_it = m_remaining_in_room.find(group_id);
    if (remaining_it == m_remaining_in_room.end() || remaining_it->second <= 0)
        return;

    auto entry_it = m_room_entry_doors.find(group_id);
    if (entry_it == m_room_entry_doors.end())
        return;

    for (entt::entity door : entry_it->second)
    {
        const entt::entity locked =
            LockDoor(*m_registry, *m_grid, door, m_locked_door_prefab_id, m_unlocked_door_prefab_id, group_id);
        if (locked != entt::null)
            m_room_cleared_doors[group_id].push_back(locked);
    }
    m_room_entry_doors.erase(entry_it);
}

bool RoomClearDoorSystem::IsEntryThresholdTile(std::uint32_t group_id, Vec2 tile) const
{
    auto entry_it = m_room_entry_doors.find(group_id);
    if (entry_it == m_room_entry_doors.end())
        return false;

    for (entt::entity door : entry_it->second)
    {
        const Position* position = m_registry->TryGetComponent<Position>(door);
        if (position && position->tile == tile)
            return true;
    }
    return false;
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
