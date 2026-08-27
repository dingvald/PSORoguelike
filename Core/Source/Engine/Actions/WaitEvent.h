#pragma once

namespace psr {

// Dispatched by WaitAction to the actor's own EventHandlerComponent around
// its no-op turn-pass. Nothing subscribes today -- a ready hook for a future
// system (e.g. HP/TP regen on wait).
struct BeforeWaitEvent
{
};

struct AfterWaitEvent
{
};

} // namespace psr
