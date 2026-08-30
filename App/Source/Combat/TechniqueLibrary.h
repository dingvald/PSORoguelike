#pragma once

#include "Combat/Technique.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace psr {

// The set of authored Technique definitions, loaded from
// App/Assets/Data/Techniques. Mirrors PhotonArtLibrary/AffixLibrary.
class TechniqueLibrary
{
public:
    TechniqueLibrary() = default;
    explicit TechniqueLibrary(std::vector<Technique> techniques);

    const Technique* Find(std::uint32_t id) const;
    const std::vector<Technique>& All() const { return m_techniques; }
    bool Empty() const { return m_techniques.empty(); }

private:
    std::vector<Technique> m_techniques;
    std::unordered_map<std::uint32_t, std::size_t> m_by_id;
};

} // namespace psr
