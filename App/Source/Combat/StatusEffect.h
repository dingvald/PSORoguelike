#pragma once

#include "Combat/StatusEffectType.h"

#include <cstdint>
#include <string>

namespace psr {

// One authored status-effect definition (e.g. "Weak Poison" vs. "Deadly
// Poison", both StatusEffectType::Poison with different magnitude/duration):
// a named ailment referenced by NameId from PhotonArt::status_effect_id/
// Technique::status_effect_id/WeaponComponent::status_effect_id. Deliberately
// minimal -- a type plus two ints -- mirroring Affix's own "single stat +
// flat amount, no effect-scripting" precedent.
//
// magnitude is reused per-type, same idiom as Affix::amount /
// PhotonArt::drain_percent: damage-per-tick-per-stack for Poison/Burn: the
// two damage-over-time types (see StatusEffectApplication.h's
// TickStatusEffects). Unused for Freeze/Shock/Confuse -- those three are
// presence-based, not magnitude-scaled (StatusEffectQueries.h's
// HasActiveStatusType is all any of them ever need: the effect is active or
// it isn't, with no amount to size).
struct StatusEffect
{
    std::uint32_t id = 0;
    std::string id_string;
    std::string name;
    StatusEffectType type = StatusEffectType::Poison;
    int magnitude = 0;
    int duration = 1; // turns; see StatusEffectApplication.h's stacking/refresh rule
};

} // namespace psr
