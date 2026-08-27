#include "Engine/Items/DropTableLibrary.h"

namespace psr {

DropTableLibrary::DropTableLibrary(std::vector<DropTable> drop_tables) : m_drop_tables(std::move(drop_tables))
{
    for (std::size_t i = 0; i < m_drop_tables.size(); ++i)
        m_by_id.emplace(m_drop_tables[i].id, i);
}

const DropTable* DropTableLibrary::Find(std::uint32_t id) const
{
    auto it = m_by_id.find(id);
    return it == m_by_id.end() ? nullptr : &m_drop_tables[it->second];
}

} // namespace psr
