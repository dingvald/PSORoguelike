#include "Engine/Dungeon/DoorLock.h"

#include "Engine/Dungeon/DoorComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"

namespace psr {

entt::entity LockDoor(Registry& registry, Grid& grid, entt::entity unlocked_door_entity,
                      std::uint32_t locked_door_prefab_id, std::uint32_t unlocked_prefab_id,
                      std::uint32_t room_index)
{
    if (!registry.IsValid(unlocked_door_entity))
        return entt::null;

    const Vec2 tile = registry.GetComponent<Position>(unlocked_door_entity).tile;

    grid.RemoveEntity(tile, unlocked_door_entity);
    registry.DestroyEntity(unlocked_door_entity);

    if (locked_door_prefab_id == 0 || !registry.HasPrefab(locked_door_prefab_id))
        return entt::null;

    const entt::entity locked_entity = registry.CreateEntity(locked_door_prefab_id);
    registry.Emplace<Position>(locked_entity, Position{tile});
    grid.AddEntity(tile, locked_entity);
    registry.Emplace<DoorComponent>(
        locked_entity, DoorComponent{DoorUnlockCondition::RoomCleared, unlocked_prefab_id, room_index, 0});
    return locked_entity;
}

} // namespace psr
