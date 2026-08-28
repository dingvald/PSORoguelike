#pragma once

#include "Engine/Math/Vec2.h"

namespace psr {

// Dispatched by MoveAction to the actor's own EventHandlerComponent around
// its Perform(). StatusEffectComponent's own handler subscribes: a Confuse
// stack overwrites offset with a random cardinal direction before
// MoveAction reads it back (mutable, same "read back after dispatch"
// contract BeforeDamageEvent::incoming_damage already establishes); a
// root/immobilize-style effect could instead set cancelled = true, though
// nothing does yet.
struct BeforeMoveEvent
{
    Vec2 offset;
    bool cancelled = false;
};

struct AfterMoveEvent
{
    Vec2 from;
    Vec2 to;
};

} // namespace psr
