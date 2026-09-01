#pragma once

#include "Engine/Actions/IAction.h"

namespace psr {

// Consumes the item at inventory_index out of the actor's InventoryComponent,
// applying its ConsumableComponent effect -- RestoreHp routes through
// HealthSystem's IncomingHealEvent, the same sole-writer pattern damage
// already uses; RestoreTp mutates TPComponent directly instead, since no
// TP-owning system exists yet to route through (see
// ConsumableComponent.h/HealEvent.h) -- then destroys the item entity.
// Constructed per-invocation with the index to use, same precedent as
// DropAction (a runtime hotbar/UI choice, not something fixed at key-bind
// time). A missing InventoryComponent, an out-of-range index, or an item
// with no ConsumableComponent is a free no-op, same "blocked = free no-op"
// convention PickupAction/DropAction already use. Dispatches
// AfterItemUseEvent on success (see ItemUseEvent.h); CombatLogBridge turns
// that into a log line and republishes the HUD's HP/TP status.
class UseItemAction : public IAction
{
public:
    static constexpr int kUseItemCost = 100;

    explicit UseItemAction(int inventory_index);

    ActionResult Perform(Entity actor) override;

private:
    int m_inventory_index;
};

} // namespace psr
