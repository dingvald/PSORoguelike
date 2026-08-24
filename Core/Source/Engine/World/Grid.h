#pragma once

#include "Engine/Math/Rect.h"
#include "Engine/Math/Vec2.h"

#include <entt/entt.hpp>

#include <cstddef>
#include <vector>

namespace psr {

// A single fixed-size 2D tile grid -- one per dungeon area, no chunking or
// streaming. Each cell can hold any number of occupying entities (e.g. a
// floor tile, a wall/socket, a decoration, and an actor can all share one
// cell), stored in the order they were added -- see AddEntity/GetEntities. A
// cell's own renderable/appearance data, if any, lives on those entities (see
// IRenderableLookup), not on the Grid itself.
class Grid
{
public:
    Grid(int width, int height);

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    Rect Bounds() const { return Rect{{0, 0}, {m_width, m_height}}; }
    bool Contains(Vec2 tile) const { return Bounds().Contains(tile); }

    // All entities occupying `tile`, in the order they were added
    // (AddEntity appends) -- callers that care about draw/stack order (see
    // TileRenderer) rely on this being insertion order, not an incidental
    // container detail. Out-of-bounds tiles return a reference to a shared
    // empty vector, so this stays allocation-free on the per-frame render
    // query path.
    const std::vector<entt::entity>& GetEntities(Vec2 tile) const;

    // Appends `entity` to `tile`'s occupant list. Does not check for
    // duplicates -- callers are expected not to add the same entity to the
    // same tile twice (mirrors the old single-slot SetEntity's lack of
    // validation).
    void AddEntity(Vec2 tile, entt::entity entity);

    // Removes the first occurrence of `entity` from `tile`'s occupant list,
    // if present; a no-op (not an error) if `entity` isn't there, so callers
    // don't need to query first.
    void RemoveEntity(Vec2 tile, entt::entity entity);

private:
    std::size_t Index(Vec2 tile) const;

    int m_width;
    int m_height;
    std::vector<std::vector<entt::entity>> m_cells;
};

} // namespace psr
