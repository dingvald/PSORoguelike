#pragma once

#include "Items/Affix.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace psr {

// The set of authored affix definitions, loaded from App/Assets/Data/Affixes.
// Mirrors DungeonLibrary/PieceLibrary.
class AffixLibrary
{
public:
    AffixLibrary() = default;
    explicit AffixLibrary(std::vector<Affix> affixes);

    const Affix* Find(std::uint32_t id) const;
    const std::vector<Affix>& All() const { return m_affixes; }
    bool Empty() const { return m_affixes.empty(); }

private:
    std::vector<Affix> m_affixes;
    std::unordered_map<std::uint32_t, std::size_t> m_by_id;
};

} // namespace psr
