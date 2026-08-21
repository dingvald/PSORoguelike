#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

#include <cstdint>
#include <string>
#include <vector>

namespace psr {

// Marks a prefab as a dungeon-piece socket: DungeonStitcher.h scans a piece's
// stamped cell prefabs for this component to find its connection points.
// tags are matched against another socket's tags (non-empty intersection) to
// decide what it can connect to; fallback_prefab_id is swapped in for a
// socket left unused by generation (a dead end), so it doesn't render as a
// dangling doorway -- 0/empty means leave the cell as-is instead. Which
// border edge a given stamp represents is placement data
// (PieceCellPrefab::edge in DungeonPiece.h), not stored here, since the same
// socket prefab can be stamped facing any direction.
struct SocketComponent
{
    std::vector<std::string> tags;
    std::uint32_t fallback_prefab_id = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<SocketComponent>("socket")
            .Data<&SocketComponent::tags>("tags")
            .Data<&SocketComponent::fallback_prefab_id>("fallback_prefab_id");
    }
};

} // namespace psr
