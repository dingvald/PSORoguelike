#pragma once

namespace psr {

// Published by HudLayer when the player clicks an inventory row on the
// Character screen; GameplayLayer subscribes and calls EquipItem -- HudLayer
// only ever needs to know which inventory index was clicked.
struct InventoryItemActivatedMessage
{
    int inventory_index = -1;
};

} // namespace psr
