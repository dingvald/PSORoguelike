#include "Items/Mag/MagCompanion.h"

#include "Components/EquipmentComponent.h"
#include "Components/LastDirectionComponent.h"
#include "Components/MagComponent.h"
#include "Components/TweenComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Math/Vec2.h"
#include "Engine/Math/Vec2f.h"
#include "Engine/World/Grid.h"

#include <algorithm>
#include <cmath>

namespace psr {

namespace {
    // A player move ever shifts the mag's trailing target by exactly one
    // tile per axis (see UpdateMagCompanion's own derivation), so any larger
    // jump -- a teleport, or a stale tile inherited across a dungeon/hub
    // transition (see the comment below) -- is treated as a teleport and
    // snapped instantly rather than lerped across the whole map.
    constexpr int kMagTeleportSnapDistance = 1;
    constexpr float kMagLerpDuration = 0.12f;

    Vec2 LastDirectionOrDefault(Registry& registry, entt::entity actor)
    {
        const LastDirectionComponent* last_direction = registry.TryGetComponent<LastDirectionComponent>(actor);
        return last_direction ? last_direction->direction : Vec2{0, 1};
    }

    // Clamps tile into grid's bounds. The mag's ideal trailing tile (one
    // tile opposite the player's last direction) can fall off the map when
    // the player is standing at an edge -- nothing about "one tile behind"
    // guarantees that tile exists. Clamping keeps every
    // Grid::AddEntity/RemoveEntity call within its own
    // assert(Contains(tile)) contract instead of indexing out of bounds.
    Vec2 ClampToGrid(const Grid& grid, Vec2 tile)
    {
        if (grid.Contains(tile))
            return tile;
        return Vec2{std::clamp(tile.x, 0, grid.GetWidth() - 1), std::clamp(tile.y, 0, grid.GetHeight() - 1)};
    }
} // namespace

void OnMagEquipped(Registry& registry, entt::entity actor, entt::entity mag_entity)
{
    const Position* actor_position = registry.TryGetComponent<Position>(actor);
    if (!actor_position)
        return;

    Grid& grid = registry.GetGrid();
    const Vec2 tile = ClampToGrid(grid, actor_position->tile - LastDirectionOrDefault(registry, actor));
    registry.Emplace<Position>(mag_entity, Position{tile});
    grid.AddEntity(tile, mag_entity);
}

void OnMagUnequipped(Registry& registry, entt::entity mag_entity)
{
    const Position* mag_position = registry.TryGetComponent<Position>(mag_entity);
    if (!mag_position)
        return;

    Grid& grid = registry.GetGrid();
    if (grid.Contains(mag_position->tile))
        grid.RemoveEntity(mag_position->tile, mag_entity);
    registry.Remove<Position>(mag_entity);
}

void UpdateMagCompanion(Registry& registry, entt::entity actor, float delta_time)
{
    const EquipmentComponent* equipment = registry.TryGetComponent<EquipmentComponent>(actor);
    if (!equipment || equipment->mag == entt::null)
        return;

    if (MagComponent* mag = registry.TryGetComponent<MagComponent>(equipment->mag))
        mag->bob_elapsed += delta_time;

    Position* mag_position = registry.TryGetComponent<Position>(equipment->mag);
    const Position* actor_position = registry.TryGetComponent<Position>(actor);
    if (!mag_position || !actor_position)
        return;

    Grid& grid = registry.GetGrid();
    const Vec2 target_tile = ClampToGrid(grid, actor_position->tile - LastDirectionOrDefault(registry, actor));
    if (target_tile == mag_position->tile)
        return;

    const Vec2 old_tile = mag_position->tile;
    const bool old_tile_in_grid = grid.Contains(old_tile);

    // Two-step move (not a Grid::MoveEntity -- none exists), same pattern
    // MoveAction itself uses. Also self-heals across a hub/dungeon
    // transition: the mag's stale tile from the old scene may not even
    // exist in the new (differently-sized) Grid, so the removal is
    // bounds-checked too rather than assumed harmless.
    if (old_tile_in_grid)
        grid.RemoveEntity(old_tile, equipment->mag);
    mag_position->tile = target_tile;
    grid.AddEntity(target_tile, equipment->mag);

    // Position snaps immediately (as above); the visible glide is a Tween
    // easing the render offset in from old_tile, same pattern MoveAction
    // uses for the player. Skipped -- rendering the snap instantly instead
    // -- when old_tile isn't even in the current Grid (a stale tile
    // inherited across a scene transition) or the jump exceeds one tile
    // (an actual teleport), so the mag never visibly slides across the map.
    const int distance = std::max(std::abs(target_tile.x - old_tile.x), std::abs(target_tile.y - old_tile.y));
    if (old_tile_in_grid && distance <= kMagTeleportSnapDistance)
    {
        registry.GetOrEmplace<TweenComponent>(equipment->mag)
            .queue.push_back(Tween{Vec2f{static_cast<float>(old_tile.x - target_tile.x),
                                         static_cast<float>(old_tile.y - target_tile.y)},
                                    Vec2f{}, kMagLerpDuration, 0.0f, nullptr});
    }
}

} // namespace psr
