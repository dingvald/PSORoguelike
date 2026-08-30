#pragma once

#include "Combat/StatusEffectType.h"
#include "Engine/ECS/Entity.h"

namespace psr {

class StatusEffectLibrary;

// True if actor currently carries at least one active stack whose authored
// StatusEffectType is type. Used by TurnCoordinator's Freeze check (every
// Step() iteration, for every queued actor) and by StatusEffectComponent's
// own Before<Action>Event handlers (Shock/Confuse) -- both call sites already
// hold a valid library reference by construction, since GameplayLayer wires
// Registry::SetStatusEffectLibrary before any turn/entity work begins.
bool HasActiveStatusType(Entity actor, const StatusEffectLibrary& library, StatusEffectType type);

} // namespace psr
