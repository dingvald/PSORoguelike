#include "Engine/World/Grid.h"

#include <algorithm>
#include <cassert>

namespace psr {

Grid::Grid(int width, int height)
    : m_width(width), m_height(height), m_cells(static_cast<std::size_t>(width) * static_cast<std::size_t>(height))
{
}

std::size_t Grid::Index(Vec2 tile) const
{
    return static_cast<std::size_t>(tile.y) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(tile.x);
}

const std::vector<entt::entity>& Grid::GetEntities(Vec2 tile) const
{
    static const std::vector<entt::entity> kEmpty;
    if (!Contains(tile))
        return kEmpty;

    return m_cells[Index(tile)];
}

void Grid::AddEntity(Vec2 tile, entt::entity entity)
{
    assert(Contains(tile));

    m_cells[Index(tile)].push_back(entity);
}

void Grid::RemoveEntity(Vec2 tile, entt::entity entity)
{
    assert(Contains(tile));

    std::vector<entt::entity>& occupants = m_cells[Index(tile)];
    const auto it = std::find(occupants.begin(), occupants.end(), entity);
    if (it != occupants.end())
        occupants.erase(it);
}

} // namespace psr
