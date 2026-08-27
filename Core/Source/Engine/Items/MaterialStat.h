#pragma once

#include "Engine/ECS/TypeReflection.h"

#include <array>
#include <string_view>
#include <utility>

namespace psr {

// Which stat a Photon crystal / material item (docs/GDD.md's "Itemization &
// Section IDs": "consumable items granting a small permanent per-stat
// boost -- Power Material -> ATP, Mind Material -> MST, HP Material -> max
// HP, etc.") permanently raises. Mirrors AffixStat's six StatsComponent
// fields plus a seventh, MaxHp -- a material can target HealthComponent's
// max_hp too, which AffixStat (weapon-bonus-only) has no reason to.
// Deliberately its own enum rather than reusing AffixStat: the two content
// families have different target semantics (a permanent character stat vs.
// a weapon's granted bonus) even though most of the value names overlap.
enum class MaterialStat
{
    Atp,
    Ata,
    Mst,
    Dfp,
    Evp,
    Lck,
    MaxHp
};

template <> struct EnumNames<MaterialStat>
{
    static constexpr std::array<std::pair<std::string_view, MaterialStat>, 7> kValues{{
        {"atp", MaterialStat::Atp},
        {"ata", MaterialStat::Ata},
        {"mst", MaterialStat::Mst},
        {"dfp", MaterialStat::Dfp},
        {"evp", MaterialStat::Evp},
        {"lck", MaterialStat::Lck},
        {"max_hp", MaterialStat::MaxHp},
    }};
};

} // namespace psr
