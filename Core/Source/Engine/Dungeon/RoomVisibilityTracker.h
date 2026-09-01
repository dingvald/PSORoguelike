#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace psr {

// Hidden: never visited. Explored: visited before, but neither the room the
// player currently occupies nor adjacent to it -- rendered dimmed, actors
// hidden (see FogOfWarRenderableLookup). Visible: the room the player
// currently occupies, or one of its immediate (socket-connected) neighbors.
enum class RoomVisibility
{
    Hidden,
    Explored,
    Visible
};

// Room-granularity fog-of-war state: which placed-piece index (see RoomMap)
// the player currently occupies, and which indices have ever been current.
// Pure logic -- no Grid/Registry dependency -- so callers drive it by
// resolving the player's tile through a RoomMap themselves (see
// GameplayLayer::OnUpdate) and passing the result in.
class RoomVisibilityTracker
{
public:
    // room_count must match the number of placed pieces (DungeonLayout::pieces
    // .size()) the accompanying RoomMap was built from -- every valid room
    // index Update()/GetVisibility() will ever see is < room_count.
    // adjacency[i] lists every room index sharing a socket connection with
    // room i (see DungeonInstantiation::room_adjacency) -- extends Visible,
    // and visited status, from the player's current room to its immediate
    // neighbors, so standing in one room renders the rooms beyond its
    // doorways too. Empty (the default) disables this: a room is only ever
    // Visible while it's current, matching the pre-adjacency behavior.
    explicit RoomVisibilityTracker(std::size_t room_count, std::vector<std::vector<std::uint32_t>> adjacency = {});

    // Marks `room` (if present) as the player's current room, and both it
    // and its adjacency-listed neighbors as visited. nullopt (player
    // standing on an untagged tile) clears the current room without
    // marking anything visited.
    void Update(std::optional<std::uint32_t> room_at_player);

    RoomVisibility GetVisibility(std::optional<std::uint32_t> room) const;

private:
    bool IsAdjacentToCurrent(std::uint32_t room) const;

    std::optional<std::uint32_t> m_current_room;
    std::vector<bool> m_visited;
    std::vector<std::vector<std::uint32_t>> m_adjacency;
};

} // namespace psr
