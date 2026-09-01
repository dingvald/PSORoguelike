#pragma once

#include "Engine/Actions/IAction.h"
#include "Engine/World/Grid.h"

namespace psr {

// Picks up every ItemComponent-tagged entity sharing the actor's current tile
// into its InventoryComponent (removing each from the Grid), up to capacity --
// any items beyond that stay on the ground, silently, same "blocked = free
// no-op" convention MoveAction already uses for a target it can't fully
// resolve. Dispatches AfterItemPickupEvent per item actually picked up (see
// ItemPickupEvent.h); CombatLogBridge already turns that into a log line.
class PickupAction : public IAction
{
public:
    static constexpr int kPickupCost = 100;

    explicit PickupAction(Grid& grid);

    ActionResult Perform(Entity actor) override;

private:
    Grid* m_grid;
};

} // namespace psr
