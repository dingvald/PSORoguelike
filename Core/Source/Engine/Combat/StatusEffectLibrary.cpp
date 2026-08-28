#include "Engine/Combat/StatusEffectLibrary.h"

namespace psr {

StatusEffectLibrary::StatusEffectLibrary(std::vector<StatusEffect> status_effects)
    : m_status_effects(std::move(status_effects))
{
    for (std::size_t i = 0; i < m_status_effects.size(); ++i)
        m_by_id.emplace(m_status_effects[i].id, i);
}

const StatusEffect* StatusEffectLibrary::Find(std::uint32_t id) const
{
    auto it = m_by_id.find(id);
    return it == m_by_id.end() ? nullptr : &m_status_effects[it->second];
}

} // namespace psr
