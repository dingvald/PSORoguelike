#pragma once

#include "Engine/Actions/IAction.h"
#include "Engine/World/Grid.h"

namespace psr {

class MessageBus;

// Picks up every ItemComponent-tagged entity sharing the actor's current tile.
// A CurrencyPickupComponent-tagged item (e.g. a dropped Meseta pile) is
// credited straight onto the actor's CurrencyComponent and destroyed --
// it never enters InventoryComponent, so it's never blocked by capacity.
// Every other item moves into InventoryComponent (removed from the Grid), up
// to capacity -- items beyond that stay on the ground, silently, same
// "blocked = free no-op" convention MoveAction already uses for a target it
// can't fully resolve. Dispatches AfterItemPickupEvent per non-currency item
// actually picked up (see ItemPickupEvent.h); CombatLogBridge already turns
// that into a log line.
class PickupAction : public IAction
{
public:
    static constexpr int kPickupCost = 100;

    PickupAction(Grid& grid, MessageBus& message_bus);

    ActionResult Perform(Entity actor) override;

private:
    Grid* m_grid;
    MessageBus* m_message_bus;
};

} // namespace psr
