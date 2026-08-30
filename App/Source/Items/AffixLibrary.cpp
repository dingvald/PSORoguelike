#include "Items/AffixLibrary.h"

namespace psr {

AffixLibrary::AffixLibrary(std::vector<Affix> affixes) : m_affixes(std::move(affixes))
{
    for (std::size_t i = 0; i < m_affixes.size(); ++i)
        m_by_id.emplace(m_affixes[i].id, i);
}

const Affix* AffixLibrary::Find(std::uint32_t id) const
{
    auto it = m_by_id.find(id);
    return it == m_by_id.end() ? nullptr : &m_affixes[it->second];
}

} // namespace psr
