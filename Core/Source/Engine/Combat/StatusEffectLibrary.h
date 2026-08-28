#pragma once

#include "Engine/Combat/StatusEffect.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace psr {

// The set of authored status-effect definitions, loaded from
// App/Assets/Data/StatusEffects. Mirrors AffixLibrary/DungeonLibrary/
// PieceLibrary.
class StatusEffectLibrary
{
public:
    StatusEffectLibrary() = default;
    explicit StatusEffectLibrary(std::vector<StatusEffect> status_effects);

    const StatusEffect* Find(std::uint32_t id) const;
    const std::vector<StatusEffect>& All() const { return m_status_effects; }
    bool Empty() const { return m_status_effects.empty(); }

private:
    std::vector<StatusEffect> m_status_effects;
    std::unordered_map<std::uint32_t, std::size_t> m_by_id;
};

} // namespace psr
