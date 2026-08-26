#pragma once

#include "Engine/Actions/ActionResult.h"
#include "Engine/ECS/Entity.h"

namespace psr {

class IAction;

// Runs action, then keeps following ActionResult::fallback (if any) until an
// attempt returns none -- only the final ActionResult's cost is ever meant
// to be applied.
//
// action is taken by reference, not ownership -- callers resolving a
// key-bound IAction (see ActionMap.h) reuse the same instance across turns,
// so ResolveAction must not destroy it. Only the fallback chain (freshly
// created per Perform() call) is ever owned/destroyed here.
ActionResult ResolveAction(IAction& action, Entity actor);

} // namespace psr
