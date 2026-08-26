#pragma once

#include "Engine/Actions/IAction.h"

namespace psr {

// Passes the turn: no state mutation, always costs a full action threshold's
// worth of energy.
class WaitAction : public IAction
{
public:
    static constexpr int kWaitCost = 100;

    ActionResult Perform(Entity actor) override;
};

} // namespace psr
