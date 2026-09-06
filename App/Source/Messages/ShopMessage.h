#pragma once

#include <string>
#include <vector>

namespace psr {

// Resolved Shop screen contents for HudLayer to render -- display names
// only, same "fully resolved" contract every other screen message already
// uses. Published by ShopState::OnEnter and re-published after a successful
// buy/sell (see Shop/ShopSnapshot.h's BuildShopMessage).
struct ShopMessage
{
    struct StockEntry
    {
        std::string display_name;
        int buy_price = 0;
        bool affordable = false;
    };

    struct SellEntry
    {
        std::string display_name;
        int sell_value = 0;
    };

    // Index-aligned with the shop's ShopStock::entries.
    std::vector<StockEntry> stock;

    // Index-aligned with the player's InventoryComponent::items.
    std::vector<SellEntry> sellable;

    int current_meseta = 0;
};

} // namespace psr
