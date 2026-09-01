#include "Engine/Dungeon/RoomMap.h"

#include <cassert>
#include <limits>

namespace psr {

namespace {
    constexpr std::uint32_t kNoRoom = std::numeric_limits<std::uint32_t>::max();
} // namespace

RoomMap::RoomMap(int width, int height)
    : m_width(width), m_height(height),
      m_room_of_tile(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), kNoRoom)
{
}

bool RoomMap::Contains(Vec2 tile) const { return tile.x >= 0 && tile.y >= 0 && tile.x < m_width && tile.y < m_height; }

std::size_t RoomMap::Index(Vec2 tile) const
{
    return static_cast<std::size_t>(tile.y) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(tile.x);
}

void RoomMap::SetRoom(Vec2 tile, std::uint32_t room_index)
{
    assert(Contains(tile));

    m_room_of_tile[Index(tile)] = room_index;
}

std::optional<std::uint32_t> RoomMap::GetRoom(Vec2 tile) const
{
    if (!Contains(tile))
        return std::nullopt;

    const std::uint32_t room = m_room_of_tile[Index(tile)];
    if (room == kNoRoom)
        return std::nullopt;
    return room;
}

} // namespace psr
