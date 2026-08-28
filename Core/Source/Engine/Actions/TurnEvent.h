#pragma once

namespace psr {

// Dispatched by TurnCoordinator to an actor's own EventHandlerComponent once
// per resolved turn (right after ResolveAction, regardless of which action
// executed -- including a Freeze-forced Wait) -- mirrors WaitEvent.h's
// empty-struct shape. StatusEffectComponent's own handler subscribes to
// this and calls TickStatusEffects (duration countdown, Poison/Burn damage).
// A lethal tick can destroy actor mid-dispatch -- TurnCoordinator checks
// Entity::IsValid() again once the dispatch returns, same as after
// resolving any other action.
struct AfterTurnEvent
{
};

} // namespace psr
