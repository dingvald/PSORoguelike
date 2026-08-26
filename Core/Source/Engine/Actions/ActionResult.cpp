#include "Engine/Actions/ActionResult.h"

#include "Engine/Actions/IAction.h"

namespace psr {

ActionResult::~ActionResult() = default;
ActionResult& ActionResult::operator=(ActionResult&&) = default;

} // namespace psr
