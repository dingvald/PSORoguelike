#pragma once

#include "Engine/Math/Vec2.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace psr {

// Maps each dungeon tile to the index (into the DungeonLayout::pieces that
// produced it) of the placed piece it belongs to -- built by
// InstantiateDungeon alongside the Grid it stamps, since a room's identity
// only exists during instantiation otherwise (see DungeonLayout::pieces'
// own doc comment). Same zero-based width/height indexing as Grid, so it's
// sized to match the Grid a given dungeon load produced.
class RoomMap
{
public:
    // Default-constructs a zero-sized map -- required so DungeonInstantiation
    // (which embeds a RoomMap) stays default-constructible as an aggregate;
    // InstantiateDungeon immediately replaces this with a properly sized one.
    RoomMap() : RoomMap(0, 0) {}
    RoomMap(int width, int height);

    // Tags `tile` as belonging to placed-piece index `room_index`. Asserts
    // `tile` is in bounds -- callers only ever tag tiles from a piece's own
    // authored footprint, which InstantiateDungeon already keeps within the
    // Grid it sized this map to match.
    void SetRoom(Vec2 tile, std::uint32_t room_index);

    // The placed-piece index `tile` belongs to, or nullopt if `tile` is
    // out of bounds or was never tagged (e.g. it's outside every placed
    // piece's footprint).
    std::optional<std::uint32_t> GetRoom(Vec2 tile) const;

private:
    bool Contains(Vec2 tile) const;
    std::size_t Index(Vec2 tile) const;

    int m_width;
    int m_height;
    std::vector<std::uint32_t> m_room_of_tile;
};

} // namespace psr
