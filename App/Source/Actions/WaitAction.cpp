#include "Actions/WaitAction.h"

namespace psr {

ActionResult WaitAction::Perform(Entity /*actor*/) { return ActionResult(kWaitCost); }

} // namespace psr
