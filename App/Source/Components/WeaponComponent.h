#pragma once

#include "Combat/Element.h"
#include "Engine/Combat/TargetingMode.h"
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
// element, prefix_affix_id/suffix_affix_id, race_bonuses, and grind_level
// are authored *base/default* values on the template -- EquipmentDropRoller
// (see App/Source/Items/EquipmentDropRoller.h) fills in element/race_bonuses/
// grind_level (bounded by max_grind_level) on the runtime clone when a
// weapon is authored without them, at drop time. The affix refs are still
// untouched by that roll -- no affix content is authored yet.
struct WeaponComponent
{
    WeaponRangeShape range_shape = WeaponRangeShape::SingleTarget;
    int range = 1;
    int hits_per_turn = 1;
    int grind_level = 0;

    // The highest grind_level a drop roll may assign this weapon (see
    // EquipmentDropRoller.cpp) -- an authored per-weapon ceiling, distinct
    // from whatever higher cap a future grinder consumable might allow past
    // this point (out of scope here; this field only bounds the drop roll).
    int max_grind_level = 5;

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
    // both its plain attacks (WeaponAttackAction, via BeforeAttackEvent) and its
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

    // How this weapon's own attack is targeted when fired explicitly (the
    // hotbar's Normal Attack slot -- see WeaponAttackAction), mirroring
    // Technique/PhotonArt's own per-content targeting_mode field. Unused for
    // a bump-triggered attack, which always swings in the move's direction.
    TargetingMode targeting_mode = TargetingMode::Directional;

    // true = a ranged weapon (e.g. a Handgun): its attack spawns a real
    // travelling ProjectileComponent instead of resolving instantly, and
    // bumping into a hostile with it equipped is a free no-op -- it must be
    // fired explicitly via the hotbar's tile-select targeting instead. False
    // (melee) weapons ignore projectile_pierces/projectile_speed entirely.
    bool fires_projectile = false;
    bool projectile_pierces = false;
    int projectile_speed = 5; // hops per full turn-cycle, same meaning as Technique::projectile_speed
    std::uint32_t projectile_prefab_id = 0; // NameId of the travelling visual entity, ignored unless fires_projectile

    // Extra energy debited from a landed hit's target, on the same 0-100
    // scale TurnQueue::kDefaultActionThreshold schedules actions with --
    // deliberately not a whole-turn "ticks" count, so a weapon can author a
    // sub-turn stun (e.g. 25 = a quarter-action delay). 0 = no stun.
    int hit_stun_energy = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<WeaponComponent>("weapon")
            .Data<&WeaponComponent::range_shape>("range_shape")
            .Data<&WeaponComponent::range>("range")
            .Data<&WeaponComponent::hits_per_turn>("hits_per_turn")
            .Data<&WeaponComponent::grind_level>("grind_level")
            .Data<&WeaponComponent::max_grind_level>("max_grind_level")
            .Data<&WeaponComponent::prefix_affix_id>("prefix_affix_id")
            .Data<&WeaponComponent::suffix_affix_id>("suffix_affix_id")
            .Data<&WeaponComponent::race_bonuses>("race_bonuses")
            .Data<&WeaponComponent::photon_art_ids>("photon_art_ids")
            .Data<&WeaponComponent::element>("element")
            .Data<&WeaponComponent::status_effect_id>("status_effect_id")
            .Data<&WeaponComponent::status_chance_percent>("status_chance_percent")
            .Data<&WeaponComponent::targeting_mode>("targeting_mode")
            .Data<&WeaponComponent::fires_projectile>("fires_projectile")
            .Data<&WeaponComponent::projectile_pierces>("projectile_pierces")
            .Data<&WeaponComponent::projectile_speed>("projectile_speed")
            .Data<&WeaponComponent::projectile_prefab_id>("projectile_prefab_id")
            .Data<&WeaponComponent::hit_stun_energy>("hit_stun_energy");
    }
};

} // namespace psr
