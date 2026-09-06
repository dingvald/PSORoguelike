#pragma once

#include "Engine/ECS/Entity.h"

namespace psr {

struct ShopStock;

// Buys stock.entries[stock_index]: debits actor's CurrencyComponent by its
// buy_price, creates a fresh instance (Registry::CreateEntity(prefab_id))
// and pushes it into actor's InventoryComponent. Free/instant, no IAction,
// same reasoning as Items/Equip.h (only reachable while the turn loop is
// already paused, see ShopState). A no-op if stock_index is out of range,
// the prefab id doesn't resolve to a registered prefab, the actor can't
// afford it, or its InventoryComponent is already at capacity. Returns
// whether anything changed.
bool BuyItem(Entity actor, const ShopStock& stock, int stock_index);

// Sells actor's InventoryComponent::items[inventory_index]: credits
// CurrencyComponent by the item's own ValueComponent::base_price (0 if it
// carries none -- the slot is still freed but nets no Meseta), removes it
// from the inventory, and destroys the entity (unlike DropAction, a sold
// item never re-enters the world). A no-op if inventory_index is out of
// range. Returns whether anything changed.
bool SellItem(Entity actor, int inventory_index);

} // namespace psr
