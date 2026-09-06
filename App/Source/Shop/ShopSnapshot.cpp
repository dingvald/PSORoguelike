#include "Shop/ShopSnapshot.h"

#include "Components/CurrencyComponent.h"
#include "Components/InventoryComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/ValueComponent.h"
#include "Items/ItemDisplayName.h"
#include "Shop/ShopStock.h"

#include <entt/core/hashed_string.hpp>

namespace psr {

ShopMessage BuildShopMessage(Registry& registry, entt::entity player, const ShopStock& stock,
                              const AffixLibrary& affixes)
{
    ShopMessage message;

    int current_meseta = 0;
    if (const CurrencyComponent* currency = registry.TryGetComponent<CurrencyComponent>(player))
        current_meseta = currency->meseta;
    message.current_meseta = current_meseta;

    message.stock.reserve(stock.entries.size());
    for (const ShopStockEntry& entry : stock.entries)
    {
        ShopMessage::StockEntry stock_entry;
        stock_entry.buy_price = entry.buy_price;
        stock_entry.affordable = current_meseta >= entry.buy_price;

        // Stock rows describe prefabs, not live instances, and FormatItemDisplayName
        // only takes a live runtime entity (a prefab's clone lives in a
        // separate entt::registry Registry has no read accessor for) --
        // spin up a throwaway clone just to read its name, then discard it.
        const std::uint32_t prefab_id = entt::hashed_string::value(entry.prefab_id_string.c_str());
        if (registry.HasPrefab(prefab_id))
        {
            const entt::entity temp = registry.CreateEntity(prefab_id);
            stock_entry.display_name = FormatItemDisplayName(registry, temp, affixes);
            registry.DestroyEntity(temp);
        }
        else
        {
            stock_entry.display_name = entry.prefab_id_string;
        }

        message.stock.push_back(std::move(stock_entry));
    }

    if (const InventoryComponent* inventory = registry.TryGetComponent<InventoryComponent>(player))
    {
        message.sellable.reserve(inventory->items.size());
        for (entt::entity item : inventory->items)
        {
            ShopMessage::SellEntry sell_entry;
            sell_entry.display_name = FormatItemDisplayName(registry, item, affixes);
            if (const ValueComponent* value = registry.TryGetComponent<ValueComponent>(item))
                sell_entry.sell_value = value->base_price;
            message.sellable.push_back(std::move(sell_entry));
        }
    }

    return message;
}

} // namespace psr
