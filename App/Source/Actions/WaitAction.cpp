#include "Actions/WaitAction.h"

#include "Engine/Actions/WaitEvent.h"

namespace psr {

ActionResult WaitAction::Perform(Entity actor)
{
    // Nothing subscribes to these yet -- a ready hook for a future system
    // (e.g. HP/TP regen on wait).
    BeforeWaitEvent before;
    actor.Dispatch(before);

    AfterWaitEvent after;
    actor.Dispatch(after);

    return ActionResult(kWaitCost);
}

} // namespace psr
