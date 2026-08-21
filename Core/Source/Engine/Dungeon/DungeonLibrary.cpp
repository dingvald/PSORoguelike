#include "Engine/Dungeon/DungeonLibrary.h"

namespace psr {

DungeonLibrary::DungeonLibrary(std::vector<Dungeon> dungeons) : m_dungeons(std::move(dungeons))
{
    for (std::size_t i = 0; i < m_dungeons.size(); ++i)
        m_by_id.emplace(m_dungeons[i].id, i);
}

const Dungeon* DungeonLibrary::Find(std::uint32_t id) const
{
    auto it = m_by_id.find(id);
    return it == m_by_id.end() ? nullptr : &m_dungeons[it->second];
}

} // namespace psr
