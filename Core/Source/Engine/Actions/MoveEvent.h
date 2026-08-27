#pragma once

#include "Engine/Math/Vec2.h"

namespace psr {

// Dispatched by MoveAction to the actor's own EventHandlerComponent around
// its Perform(). Nothing subscribes to these today -- no status-effect
// system exists yet -- they exist as a ready hook for one (e.g. a
// root/immobilize effect setting cancelled = true).
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
