#pragma once

#include "Engine/ECS/TypeReflection.h"

#include <array>
#include <string_view>
#include <utility>

namespace psr {

// The behavioral family a Photon Art/Technique's hit resolves as (docs/GDD.md's
// example families): Damage is a plain hit/damage roll; Drain additionally
// restores the caster's HP by a percentage of the damage dealt; Status
// applies its status_effect_id unconditionally on a landed hit instead of
// dealing damage (landing the cast is the check -- see StatusEffectApplication.h's
// ApplyStatusEffect, and TechniqueAction/PhotonArtAction's directional-target
// loops). GDD ascribes Drain to Photon Arts specifically, but that's
// authoring guidance, not an engine restriction -- nothing stops a Technique
// from using Drain too. Heal restores the caster's own HP instead of dealing
// damage (Resta) -- there is no ally-targeting concept in this codebase
// (Hostility.h's IsHostile is player-vs-everyone), so TechniqueAction only
// ever resolves Heal on its self-target branch; a directional Heal cast is a
// no-op, same as self-target Status being out of scope for lack of a
// buff-shaped use case.
enum class EffectFamily
{
    Damage,
    Drain,
    Status,
    Heal
};

template <> struct EnumNames<EffectFamily>
{
    static constexpr std::array<std::pair<std::string_view, EffectFamily>, 4> kValues{{
        {"damage", EffectFamily::Damage},
        {"drain", EffectFamily::Drain},
        {"status", EffectFamily::Status},
        {"heal", EffectFamily::Heal},
    }};
};

} // namespace psr
