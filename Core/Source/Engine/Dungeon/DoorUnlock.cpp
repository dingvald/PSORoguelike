#include "Engine/Dungeon/DoorUnlock.h"

#include "Engine/Dungeon/DoorComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"

namespace psr {

void UnlockDoor(Registry& registry, Grid& grid, entt::entity door_entity)
{
    if (!registry.IsValid(door_entity))
        return;

    const std::uint32_t unlocked_prefab_id = registry.GetComponent<DoorComponent>(door_entity).unlocked_prefab_id;
    const Vec2 tile = registry.GetComponent<Position>(door_entity).tile;

    grid.RemoveEntity(tile, door_entity);
    registry.DestroyEntity(door_entity);

    if (unlocked_prefab_id == 0 || !registry.HasPrefab(unlocked_prefab_id))
        return;

    const entt::entity unlocked_entity = registry.CreateEntity(unlocked_prefab_id);
    registry.Emplace<Position>(unlocked_entity, Position{tile});
    grid.AddEntity(tile, unlocked_entity);
}

} // namespace psr
