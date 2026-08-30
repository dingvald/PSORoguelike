#pragma once

#include "Engine/Math/Vec2.h"

#include <cstdint>
#include <vector>

namespace psr {

struct PendingSpawnEntry
{
    Vec2 world_cell;
    std::uint32_t prefab_id = 0;
};

// One not-yet-spawned wave for one piece placement (identified by group_id --
// see DungeonInstantiator.h). Waves before this one for the same group_id
// have already spawned or are themselves still pending, in ascending order.
struct PendingSpawnWave
{
    std::uint32_t group_id = 0;
    int wave = 0;
    std::vector<PendingSpawnEntry> entries;
};

} // namespace psr
