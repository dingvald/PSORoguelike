#pragma once

#include "Engine/Math/Rect.h"
#include "Engine/Math/Vec2.h"

#include <entt/entt.hpp>

#include <vector>

namespace psr {

// A single fixed-size 2D tile grid -- one per dungeon area, no chunking or
// streaming. Each cell holds at most one occupying entity (entt::null if
// empty); a cell's own renderable/appearance data, if any, lives on that
// entity (see IRenderableLookup), not on the Grid itself.
class Grid
{
public:
    Grid(int width, int height);

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    Rect Bounds() const { return Rect{{0, 0}, {m_width, m_height}}; }
    bool Contains(Vec2 tile) const { return Bounds().Contains(tile); }

    entt::entity GetEntity(Vec2 tile) const;
    void SetEntity(Vec2 tile, entt::entity entity);

private:
    int m_width;
    int m_height;
    std::vector<entt::entity> m_cells;
};

} // namespace psr
