#pragma once

#include "Combat/EffectFamily.h"
#include "Combat/Element.h"
#include "Components/WeaponComponent.h" // WeaponRangeShape, reused verbatim
#include "Engine/Combat/TargetingMode.h"

#include <cstdint>
#include <string>
#include <vector>

namespace psr {

// One authored {tier, power_multiplier} step -- same deferral as
// PhotonArtTier: resolution always uses tiers[0] until a per-actor usage
// counter exists (M11.1 not started).
struct TechniqueTier
{
    int tier = 1;
    float power_multiplier = 1.0f;

    template <typename V> static void Describe(V& v)
    {
        v.template Field<&TechniqueTier::tier>("tier");
        v.template Field<&TechniqueTier::power_multiplier>("power_multiplier");
    }
};

// One authored Technique: Force's TP-costed elemental spell (docs/GDD.md's
// "Elemental Techniques" section) -- learned independently of the equipped
// weapon by consuming a teach_technique ConsumableComponent item (see
// KnownTechniquesComponent.h/Items/TechniqueLearning.h), not weapon-granted
// (unlike PhotonArt, which stays purely weapon-granted via
// WeaponComponent::photon_art_ids). element is spell-authored (independent of the
// wielding weapon's own WeaponComponent::element, unlike Photon Arts, which
// inherit theirs from the weapon) -- M7.3 commits to a fixed five-element
// roster (see Element.h), superseding this field's earlier free-form NameId
// shape. Deliberately carries no drain_percent field (unlike PhotonArt):
// EffectFamily::Drain still type-checks here since the enum is shared, but
// TechniqueAction resolves it identically to Damage for lack of an amount to
// size a restore by. status_chance_percent is the M7.3 chance-on-hit for a
// Damage/Drain-family cast to also apply status_effect_id (a Status-family
// cast applies it unconditionally instead -- landing the cast is the check).
struct Technique
{
    std::uint32_t id = 0;
    std::string id_string;
    std::string name;
    int tp_cost = 0;
    Element element = Element::None;
    TargetingMode targeting_mode = TargetingMode::Directional;
    WeaponRangeShape range_shape = WeaponRangeShape::SingleTarget;
    int range = 1;
    EffectFamily effect_family = EffectFamily::Damage;
    std::uint32_t status_effect_id = 0; // NameId into StatusEffectLibrary; Status family: guaranteed on hit,
                                        // Damage/Drain family: status_chance_percent roll on hit
    int status_chance_percent = 0;      // Damage/Drain family only
    std::vector<TechniqueTier> tiers;

    // Projectile travel (see ProjectileComponent.h/ProjectileAdvanceAction.h):
    // 0 (default) means the cast resolves instantly, same as before this
    // field existed (zonde's shape); > 0 is tiles traveled per one normal
    // actor's turn -- the cast spawns a projectile_prefab_id entity instead
    // of resolving damage inline, only meaningful for range_shape
    // SingleTarget/Line and targeting_mode != Self.
    int projectile_speed = 0;
    std::uint32_t projectile_prefab_id = 0; // NameId, ignored when projectile_speed == 0
    // false: the projectile stops at the first creature or wall it reaches
    // and resolves its hit only there; true: it always travels the full
    // range_shape reach and then hits every tile along the way, same as an
    // instant Line cast's existing piercing behavior.
    bool projectile_pierces = false;

    // Which VFX prefab (if any) to spawn at the hit location once this
    // technique's damage lands -- instant or via a projectile, see
    // OnHitEffectSystem.h. 0 = no effect.
    std::uint32_t hit_effect_prefab_id = 0;
    float hit_effect_duration = 0.3f;
};

} // namespace psr
