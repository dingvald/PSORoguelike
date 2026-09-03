#include "Combat/TargetResolution.h"

#include "Components/BlocksMovementComponent.h"
#include "Engine/ECS/HealthComponent.h"

#include <cstdlib>

namespace psr {

namespace {
    // A tile blocks the attack line but is never itself a target: something
    // occupies it that blocks movement and carries no HealthComponent (a
    // wall/obstacle, as opposed to a hostile actor -- see MoveAction's own
    // BlocksMovementComponent check for the movement-side equivalent).
    bool IsWallTile(Registry& registry, const Grid& grid, Vec2 tile)
    {
        for (entt::entity occupant : grid.GetEntities(tile))
            if (registry.HasComponent<BlocksMovementComponent>(occupant) &&
                !registry.HasComponent<HealthComponent>(occupant))
                return true;
        return false;
    }

    // A tile a non-piercing projectile should stop at: something occupies it
    // that carries a HealthComponent (hostile or not -- see
    // BuildProjectilePath's own doc comment).
    bool IsCreatureTile(Registry& registry, const Grid& grid, Vec2 tile)
    {
        for (entt::entity occupant : grid.GetEntities(tile))
            if (registry.HasComponent<HealthComponent>(occupant))
                return true;
        return false;
    }
} // namespace

std::vector<Vec2> ResolveTargetTiles(const Grid& grid, Registry& registry, Vec2 origin, Vec2 direction,
                                     WeaponRangeShape shape, int range)
{
    std::vector<Vec2> tiles;
    switch (shape)
    {
    case WeaponRangeShape::SingleTarget:
    {
        const Vec2 tile = origin + direction;
        if (grid.Contains(tile))
            tiles.push_back(tile);
        break;
    }
    case WeaponRangeShape::Line:
    {
        for (int i = 1; i <= range; ++i)
        {
            const Vec2 tile = origin + direction * i;
            if (!grid.Contains(tile))
                break;
            if (IsWallTile(registry, grid, tile))
                break;
            tiles.push_back(tile);
        }
        break;
    }
    case WeaponRangeShape::Cone3:
    {
        const Vec2 perpendicular{-direction.y, direction.x};
        for (Vec2 tile : {origin + direction, origin + direction + perpendicular, origin + direction - perpendicular})
            if (grid.Contains(tile))
                tiles.push_back(tile);
        break;
    }
    case WeaponRangeShape::Surrounding:
    {
        for (Vec2 offset : {Vec2{1, 0}, Vec2{-1, 0}, Vec2{0, 1}, Vec2{0, -1}})
        {
            const Vec2 tile = origin + offset;
            if (grid.Contains(tile))
                tiles.push_back(tile);
        }
        break;
    }
    }
    return tiles;
}

std::vector<Vec2> BuildProjectilePath(const Grid& grid, Registry& registry, Vec2 origin, Vec2 direction, int range,
                                      bool pierces)
{
    std::vector<Vec2> path;
    for (int i = 1; i <= range; ++i)
    {
        const Vec2 tile = origin + direction * i;
        if (!grid.Contains(tile))
            break;
        if (IsWallTile(registry, grid, tile))
            break;
        path.push_back(tile);
        if (!pierces && IsCreatureTile(registry, grid, tile))
            break;
    }
    return path;
}

Vec2 SnapToCardinalDirection(Vec2 offset)
{
    if (offset == Vec2{0, 0})
        return offset;
    if (std::abs(offset.x) >= std::abs(offset.y))
        return Vec2{offset.x > 0 ? 1 : -1, 0};
    return Vec2{0, offset.y > 0 ? 1 : -1};
}

} // namespace psr
