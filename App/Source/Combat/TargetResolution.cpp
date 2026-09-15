#include "Combat/TargetResolution.h"

#include "Combat/Hostility.h"
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

    // The `count` grid tiles a Bresenham line from origin through target
    // follows, continuing past target along the same slope rather than
    // stopping there -- an any-angle generalization of the direction*i
    // stepping ResolveTargetTiles/BuildProjectilePath use for their fixed
    // 8-way directions. Empty when origin == target (no slope to continue).
    std::vector<Vec2> BresenhamRay(Vec2 origin, Vec2 target, int count)
    {
        std::vector<Vec2> tiles;
        if (origin == target || count <= 0)
            return tiles;

        int dx = target.x - origin.x;
        int dy = target.y - origin.y;
        const int sx = (dx > 0) - (dx < 0);
        const int sy = (dy > 0) - (dy < 0);
        dx = std::abs(dx);
        dy = std::abs(dy);

        int x = origin.x;
        int y = origin.y;
        int error = 0;
        tiles.reserve(count);
        if (dx >= dy)
        {
            for (int i = 0; i < count; ++i)
            {
                x += sx;
                error += dy;
                if (2 * error >= dx)
                {
                    y += sy;
                    error -= dx;
                }
                tiles.push_back(Vec2{x, y});
            }
        }
        else
        {
            for (int i = 0; i < count; ++i)
            {
                y += sy;
                error += dx;
                if (2 * error >= dy)
                {
                    x += sx;
                    error -= dy;
                }
                tiles.push_back(Vec2{x, y});
            }
        }
        return tiles;
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
        std::vector<Vec2> candidates{origin + direction};
        if (direction.x != 0 && direction.y != 0)
        {
            // Diagonal direction: flank with its two cardinal components
            // (e.g. numpad 9 -> numpad 8 and numpad 6), rather than the
            // perpendicular-vector math below, which only produces the
            // correct flanking tiles when direction is itself cardinal.
            candidates.push_back(origin + Vec2{direction.x, 0});
            candidates.push_back(origin + Vec2{0, direction.y});
        }
        else
        {
            const Vec2 perpendicular{-direction.y, direction.x};
            candidates.push_back(origin + direction + perpendicular);
            candidates.push_back(origin + direction - perpendicular);
        }
        for (Vec2 tile : candidates)
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

std::vector<Vec2> ResolveTargetTilesToward(const Grid& grid, Registry& registry, Vec2 origin, Vec2 target,
                                           WeaponRangeShape shape, int range)
{
    switch (shape)
    {
    case WeaponRangeShape::SingleTarget:
    {
        std::vector<Vec2> tiles;
        if (grid.Contains(target))
            tiles.push_back(target);
        return tiles;
    }
    case WeaponRangeShape::Line:
    {
        std::vector<Vec2> tiles;
        for (Vec2 tile : BresenhamRay(origin, target, range))
        {
            if (!grid.Contains(tile))
                break;
            if (IsWallTile(registry, grid, tile))
                break;
            tiles.push_back(tile);
        }
        return tiles;
    }
    case WeaponRangeShape::Cone3:
    case WeaponRangeShape::Surrounding:
    default:
        return ResolveTargetTiles(grid, registry, origin, SnapToDirection(target - origin), shape, range);
    }
}

std::vector<Vec2> BuildProjectilePathToward(const Grid& grid, Registry& registry, Vec2 origin, Vec2 target, int range,
                                            bool pierces)
{
    std::vector<Vec2> path;
    for (Vec2 tile : BresenhamRay(origin, target, range))
    {
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

bool HasLineOfSight(const Grid& grid, Registry& registry, Vec2 from, Vec2 to)
{
    int x0 = from.x;
    int y0 = from.y;
    const int x1 = to.x;
    const int y1 = to.y;
    const int dx = std::abs(x1 - x0);
    const int dy = -std::abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (x0 != x1 || y0 != y1)
    {
        const int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }

        if ((x0 != x1 || y0 != y1) && IsWallTile(registry, grid, Vec2{x0, y0}))
            return false;
    }
    return true;
}

Vec2 SnapToDirection(Vec2 offset)
{
    if (offset == Vec2{0, 0})
        return offset;
    const int sx = (offset.x > 0) - (offset.x < 0);
    const int sy = (offset.y > 0) - (offset.y < 0);
    if (offset.x == 0)
        return Vec2{0, sy};
    if (offset.y == 0)
        return Vec2{sx, 0};

    // tan(22.5 deg): the boundary between "mostly axis-aligned" (snaps to a
    // cardinal direction) and "roughly diagonal" (snaps to sx,sy).
    constexpr float kHalfOctaveTan = 0.41421356f;
    const float ax = static_cast<float>(std::abs(offset.x));
    const float ay = static_cast<float>(std::abs(offset.y));
    if (ay < ax * kHalfOctaveTan)
        return Vec2{sx, 0};
    if (ax < ay * kHalfOctaveTan)
        return Vec2{0, sy};
    return Vec2{sx, sy};
}

bool IsWalkableStep(const Grid& grid, Registry& registry, Entity actor, Vec2 tile)
{
    if (!grid.Contains(tile))
        return false;

    bool blocked = false;
    for (entt::entity occupant : grid.GetEntities(tile))
    {
        if (!registry.HasComponent<BlocksMovementComponent>(occupant))
            continue;
        blocked = true;

        if (!registry.HasComponent<HealthComponent>(occupant))
            continue;
        if (IsHostile(actor, Entity(registry, occupant)))
            return true;
    }
    return !blocked;
}

} // namespace psr
