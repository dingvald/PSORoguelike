#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/TypeReflection.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace psr {

// How a weapon's attack reaches its target(s) (docs/GDD.md's melee "range
// shape: single adjacent tile / cone / line" and ranged "range, spread,
// hits-per-turn" vocabulary, unified into one enum): SingleTarget hits one
// adjacent tile, Cone3 hits three adjacent tiles (a Hunter cone), Surrounding
// hits every adjacent tile at once, Line reaches out `range` tiles in a
// straight line (a Ranger weapon). `range`/`hits_per_turn` are only
// meaningful for the shapes that use them (Line's distance, a multi-hit
// Ranger's hits-per-turn) -- combat resolution (M7.1) is what actually reads
// these, not this schema.
enum class WeaponRangeShape
{
    SingleTarget,
    Cone3,
    Surrounding,
    Line
};

template <> struct EnumNames<WeaponRangeShape>
{
    static constexpr std::array<std::pair<std::string_view, WeaponRangeShape>, 4> kValues{{
        {"single_target", WeaponRangeShape::SingleTarget},
        {"cone_3", WeaponRangeShape::Cone3},
        {"surrounding", WeaponRangeShape::Surrounding},
        {"line", WeaponRangeShape::Line},
    }};
};

// One authored {race, bonus%} entry. A list rather than one-per-race fixed
// fields, matching RaceComponent's own "no fixed enum, purely data-driven"
// design intent -- a weapon can carry zero, one, or several race bonuses.
struct RaceBonusEntry
{
    std::uint32_t race_id = 0;
    int bonus_percent = 0;

    template <typename V> static void Describe(V& v)
    {
        v.template Field<&RaceBonusEntry::race_id>("race_id");
        v.template Field<&RaceBonusEntry::bonus_percent>("bonus_percent");
    }
};

// A weapon prefab's non-stat fields. A weapon entity also carries a sibling
// StatsComponent (reattached here to mean "stat bonus granted when
// equipped," not an entity's own base stats) and RarityComponent.
//
// grind_level, race_bonuses, and the affix refs are authored *base/default*
// values on the template -- randomly rolling race_bonuses and applying a
// monogrinder consumable to raise grind_level are both M8.2/drop-table
// concerns that don't exist yet; this schema only holds the data.
struct WeaponComponent
{
    WeaponRangeShape range_shape = WeaponRangeShape::SingleTarget;
    int range = 1;
    int hits_per_turn = 1;
    int grind_level = 0;
    std::uint32_t prefix_affix_id = 0; // NameId into the Affix library, 0 = none
    std::uint32_t suffix_affix_id = 0; // NameId into the Affix library, 0 = none
    std::vector<RaceBonusEntry> race_bonuses;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<WeaponComponent>("weapon")
            .Data<&WeaponComponent::range_shape>("range_shape")
            .Data<&WeaponComponent::range>("range")
            .Data<&WeaponComponent::hits_per_turn>("hits_per_turn")
            .Data<&WeaponComponent::grind_level>("grind_level")
            .Data<&WeaponComponent::prefix_affix_id>("prefix_affix_id")
            .Data<&WeaponComponent::suffix_affix_id>("suffix_affix_id")
            .Data<&WeaponComponent::race_bonuses>("race_bonuses");
    }
};

} // namespace psr
