#pragma once

#include "Engine/Actions/ActionResult.h"
#include "Engine/ECS/Entity.h"

namespace psr {

// Everything an actor can do implements this. Perform() should mutate only
// what's necessary to carry out this action itself (e.g. MoveAction touches
// position/Grid membership, nothing else) -- turn cost is reported via the
// returned ActionResult, not applied here; ResolveAction (ActionExecutor.h)
// and whoever calls it own applying that cost.
class IAction
{
public:
    virtual ~IAction() = default;

    virtual ActionResult Perform(Entity actor) = 0;
};

} // namespace psr
