#pragma once

#include "Engine/ECS/TypeReflection.h"

#include <array>
#include <string_view>
#include <utility>

namespace psr {

// The behavioral family a Photon Art/Technique's hit resolves as (docs/GDD.md's
// example families): Damage is a plain hit/damage roll; Drain additionally
// restores the caster's HP by a percentage of the damage dealt; Status ships
// its status_effect_id unconsumed for now (the actual tick/duration/cure
// framework is M7.3's job). GDD ascribes Drain to Photon Arts specifically,
// but that's authoring guidance, not an engine restriction -- nothing stops a
// Technique from using Drain too.
enum class EffectFamily
{
    Damage,
    Drain,
    Status
};

template <> struct EnumNames<EffectFamily>
{
    static constexpr std::array<std::pair<std::string_view, EffectFamily>, 3> kValues{{
        {"damage", EffectFamily::Damage},
        {"drain", EffectFamily::Drain},
        {"status", EffectFamily::Status},
    }};
};

} // namespace psr
