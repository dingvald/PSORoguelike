#pragma once

#include "Engine/Actions/IAction.h"
#include "Engine/World/Grid.h"

namespace psr {

// Drops the item at inventory_index out of the actor's InventoryComponent
// onto its current tile. Constructed per-invocation with the index to drop
// rather than bound once in ActionMap -- same precedent as PhotonArtAction/
// TechniqueAction's targeting flow (see TurnCoordinator.h), since which index
// to drop is a runtime UI choice, not something fixed at key-bind time. A
// missing InventoryComponent or an out-of-range index is a free no-op.
// Dispatches AfterItemDropEvent on success (see ItemDropEvent.h);
// CombatLogBridge turns that into a log line.
class DropAction : public IAction
{
public:
    static constexpr int kDropCost = 100;

    DropAction(Grid& grid, int inventory_index);

    ActionResult Perform(Entity actor) override;

private:
    Grid* m_grid;
    int m_inventory_index;
};

} // namespace psr
