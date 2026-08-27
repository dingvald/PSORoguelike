#include "Engine/Combat/TechniqueLibrary.h"

namespace psr {

TechniqueLibrary::TechniqueLibrary(std::vector<Technique> techniques) : m_techniques(std::move(techniques))
{
    for (std::size_t i = 0; i < m_techniques.size(); ++i)
        m_by_id.emplace(m_techniques[i].id, i);
}

const Technique* TechniqueLibrary::Find(std::uint32_t id) const
{
    auto it = m_by_id.find(id);
    return it == m_by_id.end() ? nullptr : &m_techniques[it->second];
}

} // namespace psr
