#pragma once

namespace psr {

// Published by HudLayer when the player picks a Storage screen Inventory
// row; GameplayLayer subscribes and calls Items/Storage.h's StoreItem.
struct StorageItemActivatedMessage
{
    int inventory_index = -1;
};

} // namespace psr
