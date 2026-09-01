#include "Engine/Dungeon/RoomVisibilityTracker.h"

#include <algorithm>

namespace psr {

RoomVisibilityTracker::RoomVisibilityTracker(std::size_t room_count, std::vector<std::vector<std::uint32_t>> adjacency)
    : m_visited(room_count, false), m_adjacency(std::move(adjacency))
{
}

void RoomVisibilityTracker::Update(std::optional<std::uint32_t> room_at_player)
{
    m_current_room = room_at_player;
    if (!room_at_player || *room_at_player >= m_visited.size())
        return;

    m_visited[*room_at_player] = true;
    if (*room_at_player < m_adjacency.size())
        for (std::uint32_t neighbor : m_adjacency[*room_at_player])
            if (neighbor < m_visited.size())
                m_visited[neighbor] = true;
}

bool RoomVisibilityTracker::IsAdjacentToCurrent(std::uint32_t room) const
{
    if (!m_current_room || *m_current_room >= m_adjacency.size())
        return false;
    const std::vector<std::uint32_t>& neighbors = m_adjacency[*m_current_room];
    return std::find(neighbors.begin(), neighbors.end(), room) != neighbors.end();
}

RoomVisibility RoomVisibilityTracker::GetVisibility(std::optional<std::uint32_t> room) const
{
    if (!room)
        return RoomVisibility::Hidden;
    if (room == m_current_room || IsAdjacentToCurrent(*room))
        return RoomVisibility::Visible;
    if (*room < m_visited.size() && m_visited[*room])
        return RoomVisibility::Explored;
    return RoomVisibility::Hidden;
}

} // namespace psr
