#pragma once

#include "Combat/Element.h"
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

    // Which Photon Arts this weapon grants (NameIds into PhotonArtLibrary) --
    // weapon-attached, not character-learned, per PhotonArt.h's own doc
    // comment. A Saber/Handgun would list photon_art_ids -- the engine
    // doesn't enforce which weapon category may carry it, that's a
    // content-authoring convention. Techniques, unlike Photon Arts, are no
    // longer weapon-granted at all -- see Technique.h/KnownTechniquesComponent.h.
    std::vector<std::uint32_t> photon_art_ids;

    // The weapon's own elemental flavor (e.g. a "Fire Saber"): inherited by
    // both its plain attacks (AttackAction, via BeforeAttackEvent) and its
    // granted Photon Arts (PhotonArtAction, via BeforePhotonArtCastEvent) --
    // a Photon Art is "channeled through" its granting weapon, per
    // Technique.h's own doc comment on how Photon Arts/Techniques are
    // granted. status_effect_id (NameId into StatusEffectLibrary) is the
    // ailment status_chance_percent has a chance to apply on a landed hit,
    // when element != None. A Technique's own element/status_effect_id (see
    // Technique.h) are spell-authored instead, independent of the wielding
    // weapon.
    Element element = Element::None;
    std::uint32_t status_effect_id = 0;
    int status_chance_percent = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<WeaponComponent>("weapon")
            .Data<&WeaponComponent::range_shape>("range_shape")
            .Data<&WeaponComponent::range>("range")
            .Data<&WeaponComponent::hits_per_turn>("hits_per_turn")
            .Data<&WeaponComponent::grind_level>("grind_level")
            .Data<&WeaponComponent::prefix_affix_id>("prefix_affix_id")
            .Data<&WeaponComponent::suffix_affix_id>("suffix_affix_id")
            .Data<&WeaponComponent::race_bonuses>("race_bonuses")
            .Data<&WeaponComponent::photon_art_ids>("photon_art_ids")
            .Data<&WeaponComponent::element>("element")
            .Data<&WeaponComponent::status_effect_id>("status_effect_id")
            .Data<&WeaponComponent::status_chance_percent>("status_chance_percent");
    }
};

} // namespace psr
