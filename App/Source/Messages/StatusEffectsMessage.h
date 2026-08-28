#pragma once

#include "Engine/Combat/StatusEffectType.h"

#include <vector>

namespace psr {

// Player active-status snapshot for HudLayer to render icon+duration chips
// from. Plain values only -- no Entity/Registry -- published by
// CombatLogBridge whenever the player's StatusEffectComponent changes (see
// AfterStatusEffectsChangedEvent), same convention as PlayerStatusMessage.
struct StatusEffectsMessage
{
    struct ActiveEntry
    {
        StatusEffectType type = StatusEffectType::Poison;
        int stacks = 0;
        int remaining_duration = 0;
    };

    std::vector<ActiveEntry> active;
};

} // namespace psr
