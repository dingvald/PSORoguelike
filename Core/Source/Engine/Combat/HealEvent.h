#pragma once

#include "Engine/ECS/Entity.h"

namespace psr {

// Dispatched by a heal source (UseItemAction, so far) to the *target
// itself* -- Entity::Dispatch, i.e. "self", same convention
// IncomingDamageEvent uses (see DamageEvent.h). HealthSystem is the sole
// intended handler: it applies amount to the target's own HealthComponent,
// clamped to max_hp, and dispatches AfterHealEvent back at source on its
// behalf. No BeforeHealEvent/mitigation step -- unlike damage, nothing
// needs to intercept or reduce a heal yet.
struct IncomingHealEvent
{
    Entity source; // who to dispatch AfterHealEvent at once this is applied
    int amount = 0;
};

// Dispatched by HealthSystem back at a heal's source once it's applied,
// same convention as AfterDamageEvent. amount is the amount actually
// applied (post max_hp clamp), not necessarily the event's requested
// amount.
struct AfterHealEvent
{
    Entity target;
    int amount = 0;
};

} // namespace psr
