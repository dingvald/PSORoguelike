#pragma once

#include "Engine/ECS/TypeReflection.h"

#include <array>
#include <string_view>
#include <utility>

namespace psr {

// The fixed ailment roster (ROADMAP.md's M7.3 bullet: "Freeze/Poison/Shock/
// Confuse framework", plus Burn added alongside them per the user's explicit
// request). Poison/Burn are magnitude-scaled damage-over-time (see
// StatusEffect::magnitude); Freeze/Shock/Confuse are purely presence-based --
// their behavior is "this status is active or it isn't," with no amount to
// scale (see StatusEffectQueries.h's HasActiveStatusType, which is all any of
// the three ever need).
enum class StatusEffectType
{
    Poison,
    Burn,
    Freeze,
    Shock,
    Confuse
};

template <> struct EnumNames<StatusEffectType>
{
    static constexpr std::array<std::pair<std::string_view, StatusEffectType>, 5> kValues{{
        {"poison", StatusEffectType::Poison},
        {"burn", StatusEffectType::Burn},
        {"freeze", StatusEffectType::Freeze},
        {"shock", StatusEffectType::Shock},
        {"confuse", StatusEffectType::Confuse},
    }};
};

} // namespace psr
