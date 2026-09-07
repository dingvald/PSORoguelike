#pragma once

namespace psr {

// Published by HudLayer when the player picks a Shop screen "Your Items"
// row; GameplayLayer subscribes and calls Items/Shop.h's SellItem.
struct ShopSellRequestedMessage
{
    int inventory_index = -1;
};

} // namespace psr
