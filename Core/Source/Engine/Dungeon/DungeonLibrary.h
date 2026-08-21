#pragma once

#include "Engine/Dungeon/Dungeon.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace psr {

// The set of authored dungeon definitions, loaded from
// App/Assets/Data/Dungeons. Mirrors PieceLibrary/UnnamedRoguelike's
// BiomeLibrary.
class DungeonLibrary
{
public:
    DungeonLibrary() = default;
    explicit DungeonLibrary(std::vector<Dungeon> dungeons);

    const Dungeon* Find(std::uint32_t id) const;
    const std::vector<Dungeon>& All() const { return m_dungeons; }
    bool Empty() const { return m_dungeons.empty(); }

private:
    std::vector<Dungeon> m_dungeons;
    std::unordered_map<std::uint32_t, std::size_t> m_by_id;
};

} // namespace psr
