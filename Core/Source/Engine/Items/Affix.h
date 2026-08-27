#pragma once

#include "Engine/ECS/TypeReflection.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace psr {

// Whether an affix authors as a weapon's prefix or suffix modifier
// (WeaponComponent::prefix_affix_id/suffix_affix_id).
enum class AffixKind
{
    Prefix,
    Suffix
};

template <> struct EnumNames<AffixKind>
{
    static constexpr std::array<std::pair<std::string_view, AffixKind>, 2> kValues{{
        {"prefix", AffixKind::Prefix},
        {"suffix", AffixKind::Suffix},
    }};
};

// Which stat an affix bumps -- mirrors StatsComponent's six fields.
enum class AffixStat
{
    Atp,
    Ata,
    Mst,
    Dfp,
    Evp,
    Lck
};

template <> struct EnumNames<AffixStat>
{
    static constexpr std::array<std::pair<std::string_view, AffixStat>, 6> kValues{{
        {"atp", AffixStat::Atp},
        {"ata", AffixStat::Ata},
        {"mst", AffixStat::Mst},
        {"dfp", AffixStat::Dfp},
        {"evp", AffixStat::Evp},
        {"lck", AffixStat::Lck},
    }};
};

// One authored weapon prefix/suffix definition (e.g. PSO-style "Power" or
// "God"): a named modifier granting a flat bonus to one stat. Deliberately
// minimal -- a single stat + flat amount, no effect-scripting -- since
// nothing consumes an affix's effect yet (item generation/drop rolling is
// M8.2, not yet started); this only needs to round-trip as data for now,
// same reasoning as Dungeon's DungeonLockConfig.
struct Affix
{
    std::uint32_t id = 0;
    std::string id_string;
    std::string name;
    AffixKind kind = AffixKind::Prefix;
    AffixStat stat = AffixStat::Atp;
    int amount = 0;
};

} // namespace psr
