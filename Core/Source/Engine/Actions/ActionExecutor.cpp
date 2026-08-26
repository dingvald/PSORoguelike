#include "Engine/Actions/ActionExecutor.h"

#include "Engine/Actions/IAction.h"

namespace psr {

ActionResult ResolveAction(IAction& action, Entity actor)
{
    ActionResult result = action.Perform(actor);
    while (result.fallback)
    {
        std::unique_ptr<IAction> next = std::move(result.fallback);
        result = next->Perform(actor);
    }
    return result;
}

} // namespace psr
