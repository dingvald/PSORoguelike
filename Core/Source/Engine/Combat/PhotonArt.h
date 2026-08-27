#pragma once

#include "Engine/Combat/EffectFamily.h"
#include "Engine/Combat/TargetingMode.h"
#include "Engine/ECS/WeaponComponent.h" // WeaponRangeShape, reused verbatim

#include <cstdint>
#include <string>
#include <vector>

namespace psr {

// One authored {tier, power_multiplier} step. "Tiered by use" (per docs/GDD.md)
// needs a per-actor usage counter that doesn't exist yet (no XP/leveling
// system -- M11.1 not started), so resolution always uses tiers[0] (or a flat
// 1.0 multiplier if empty) for now; this only needs to round-trip as data,
// same deferral M5.2's spawn_weight already makes for the same reason.
struct PhotonArtTier
{
    int tier = 1;
    float power_multiplier = 1.0f;

    template <typename V> static void Describe(V& v)
    {
        v.template Field<&PhotonArtTier::tier>("tier");
        v.template Field<&PhotonArtTier::power_multiplier>("power_multiplier");
    }
};

// One authored Photon Art: a weapon-attached, PP-costed special attack
// (docs/GDD.md's "Weapons, Photon Arts & combat skills" section) -- granted by
// whichever weapon(s) list its id in WeaponComponent::photon_art_ids, not
// learned by the character. Deliberately minimal per the GDD's own
// "exact effects/naming are a follow-on balancing pass" framing: drain_percent
// and status_effect_id are effect-family-specific fields that sit unused when
// effect_family doesn't call for them, rather than three separate structs for
// three families that otherwise share every other field.
struct PhotonArt
{
    std::uint32_t id = 0;
    std::string id_string;
    std::string name;
    int pp_cost = 0;
    TargetingMode targeting_mode = TargetingMode::Directional;
    WeaponRangeShape range_shape = WeaponRangeShape::SingleTarget;
    int range = 1;
    int hits_per_turn = 1;
    EffectFamily effect_family = EffectFamily::Damage;
    int drain_percent = 0;              // EffectFamily::Drain only
    std::uint32_t status_effect_id = 0; // EffectFamily::Status only; unconsumed until M7.3
    std::vector<PhotonArtTier> tiers;
};

} // namespace psr
