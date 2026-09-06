#pragma once

namespace psr {

// Published by HudLayer when the player picks a Shop screen stock row;
// GameplayLayer subscribes and calls Items/Shop.h's BuyItem.
struct ShopBuyRequestedMessage
{
    int stock_index = -1;
};

} // namespace psr
