#include "Engine/World/Grid.h"

#include <cassert>

namespace psr {

Grid::Grid(int width, int height)
    : m_width(width), m_height(height), m_cells(static_cast<std::size_t>(width) * static_cast<std::size_t>(height))
{
    // Not passed to the vector's fill-constructor: entt::null's templated
    // conversion operator makes it ambiguous against std::vector's
    // (size_type, const Allocator&) overload. Assignment, not construction,
    // is unambiguous.
    for (entt::entity& cell : m_cells)
        cell = entt::null;
}

entt::entity Grid::GetEntity(Vec2 tile) const
{
    if (!Contains(tile))
        return entt::null;

    return m_cells[static_cast<std::size_t>(tile.y) * static_cast<std::size_t>(m_width) +
                   static_cast<std::size_t>(tile.x)];
}

void Grid::SetEntity(Vec2 tile, entt::entity entity)
{
    assert(Contains(tile));

    m_cells[static_cast<std::size_t>(tile.y) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(tile.x)] =
        entity;
}

} // namespace psr
