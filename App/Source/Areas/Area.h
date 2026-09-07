#pragma once

#include "Engine/ECS/TypeReflection.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace psr {

// The environmental hazard an area theme applies (PSO-analogous trap/damage
// flavor per area: Caves' poison gas, Mines' electric traps, Ruins' dark
// damage floors, ...). A fixed, permanently-named roster like Element/
// SectionId, not an open NameId like RaceComponent's races -- nothing
// consumes a hazard yet (no hazard mechanic exists), so this only needs to
// round-trip as data for now, same "just needs to round-trip" precedent
// DungeonLockConfig/Affix::amount already set.
enum class HazardType
{
    None,
    Poison,
    Electric,
    Fire,
    Dark
};

template <> struct EnumNames<HazardType>
{
    static constexpr std::array<std::pair<std::string_view, HazardType>, 5> kValues{{
        {"none", HazardType::None},
        {"poison", HazardType::Poison},
        {"electric", HazardType::Electric},
        {"fire", HazardType::Fire},
        {"dark", HazardType::Dark},
    }};
};

// One authored area theme (PSO-analogous Forest/Caves/Mines/Ruins): a
// display name, dominant race, hazard type, a small floor/wall/accent tile
// palette, and which area must be cleared before this one unlocks (M4.5).
// Lives in App, not Core, alongside RaceComponent -- Dungeon/DungeonPiece
// (Core) stay theme-agnostic, carrying area_tag as a bare filter string with
// no meaning of its own; Area is what gives that string real content, the
// same relationship RaceComponent already has with Core's generic NameId
// race fields.
//
// `tag` -- not `id_string` -- is what Dungeon::area_tag/DungeonPiece::area_tag
// match against: those compare as a plain filter string (see DungeonStitcher's
// area_tag equality checks), not a NameIdRegistry-resolved reference like
// every other cross-content link in this project, so Area needs its own
// plain-string field to match on rather than reusing its file-derived id.
struct Area
{
    std::uint32_t id = 0;
    std::string id_string;
    std::string name;
    std::string tag;
    std::uint32_t race_id = 0;
    HazardType hazard = HazardType::None;
    std::uint32_t floor_texture_id = 0;
    std::uint32_t wall_texture_id = 0;
    std::uint32_t accent_texture_id = 0;
    std::string unlock_predecessor_tag; // empty = unlocked from the start, no predecessor
};

} // namespace psr
