#include "Items/Shop.h"

#include "Components/CurrencyComponent.h"
#include "Components/InventoryComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/ValueComponent.h"
#include "Shop/ShopStock.h"

#include <entt/core/hashed_string.hpp>

#include <cstddef>

namespace psr {

bool BuyItem(Entity actor, const ShopStock& stock, int stock_index)
{
    if (stock_index < 0 || stock_index >= static_cast<int>(stock.entries.size()))
        return false;

    const ShopStockEntry& entry = stock.entries[static_cast<std::size_t>(stock_index)];
    Registry& registry = actor.GetRegistry();
    const std::uint32_t prefab_id = entt::hashed_string::value(entry.prefab_id_string.c_str());
    if (!registry.HasPrefab(prefab_id))
        return false;

    CurrencyComponent* currency = actor.TryGet<CurrencyComponent>();
    if (!currency || currency->meseta < entry.buy_price)
        return false;

    InventoryComponent& inventory = actor.GetOrEmplace<InventoryComponent>();
    if (static_cast<int>(inventory.items.size()) >= inventory.capacity)
        return false;

    currency->meseta -= entry.buy_price;
    inventory.items.push_back(registry.CreateEntity(prefab_id));
    return true;
}

bool SellItem(Entity actor, int inventory_index)
{
    InventoryComponent* inventory = actor.TryGet<InventoryComponent>();
    if (!inventory || inventory_index < 0 || inventory_index >= static_cast<int>(inventory->items.size()))
        return false;

    Registry& registry = actor.GetRegistry();
    const entt::entity item = inventory->items[static_cast<std::size_t>(inventory_index)];

    int sell_value = 0;
    if (const ValueComponent* value = registry.TryGetComponent<ValueComponent>(item))
        sell_value = value->base_price;

    CurrencyComponent& currency = actor.GetOrEmplace<CurrencyComponent>();
    currency.meseta += sell_value;

    inventory->items.erase(inventory->items.begin() + inventory_index);
    registry.DestroyEntity(item);
    return true;
}

} // namespace psr
