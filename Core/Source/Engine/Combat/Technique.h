#pragma once

#include "Engine/Combat/EffectFamily.h"
#include "Engine/Combat/Element.h"
#include "Engine/Combat/TargetingMode.h"
#include "Engine/ECS/WeaponComponent.h" // WeaponRangeShape, reused verbatim

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
// "Elemental Techniques" section) -- granted by whichever weapon(s) list its
// id in WeaponComponent::technique_ids (a Wand/Cane, per the GDD's "Wand is
// the channel Techniques are cast through"), not learned independently of
// equipment -- the GDD never commits to a separate learning mechanism, so
// this avoids inventing one. element is spell-authored (independent of the
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
};

} // namespace psr
