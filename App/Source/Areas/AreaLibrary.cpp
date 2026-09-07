#include "Areas/AreaLibrary.h"

namespace psr {

AreaLibrary::AreaLibrary(std::vector<Area> areas) : m_areas(std::move(areas))
{
    for (std::size_t i = 0; i < m_areas.size(); ++i)
    {
        m_by_id.emplace(m_areas[i].id, i);
        if (!m_areas[i].tag.empty())
            m_by_tag.emplace(m_areas[i].tag, i);
    }
}

const Area* AreaLibrary::Find(std::uint32_t id) const
{
    auto it = m_by_id.find(id);
    return it == m_by_id.end() ? nullptr : &m_areas[it->second];
}

const Area* AreaLibrary::FindByTag(const std::string& tag) const
{
    if (tag.empty())
        return nullptr;
    auto it = m_by_tag.find(tag);
    return it == m_by_tag.end() ? nullptr : &m_areas[it->second];
}

} // namespace psr
