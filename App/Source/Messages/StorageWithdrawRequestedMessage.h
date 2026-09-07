#pragma once

namespace psr {

// Published by HudLayer when the player picks a Storage screen Storage row;
// GameplayLayer subscribes and calls Items/Storage.h's WithdrawItem.
struct StorageWithdrawRequestedMessage
{
    int storage_index = -1;
};

} // namespace psr
