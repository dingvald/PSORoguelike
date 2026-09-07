#pragma once

#include "Areas/Area.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace psr {

// The set of authored area definitions, loaded from App/Assets/Data/Areas.
// Mirrors AffixLibrary/DungeonLibrary.
class AreaLibrary
{
public:
    AreaLibrary() = default;
    explicit AreaLibrary(std::vector<Area> areas);

    const Area* Find(std::uint32_t id) const;

    // Looks an area up by its authored `tag` -- the plain string
    // Dungeon::area_tag/DungeonPiece::area_tag match against (see Area.h's
    // own doc comment for why this isn't the same as Find's id). Returns
    // nullptr for an empty tag or one with no matching area.
    const Area* FindByTag(const std::string& tag) const;

    const std::vector<Area>& All() const { return m_areas; }
    bool Empty() const { return m_areas.empty(); }

private:
    std::vector<Area> m_areas;
    std::unordered_map<std::uint32_t, std::size_t> m_by_id;
    std::unordered_map<std::string, std::size_t> m_by_tag;
};

} // namespace psr
