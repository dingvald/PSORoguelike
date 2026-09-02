#pragma once

#include <optional>

namespace psr {

// Published by HudLayer's RmlHoverListener on an inventory row's mouseover
// ({index}) or mouseout (nullopt). GameplayLayer stores the value and
// republishes the Character screen so its stats panel shows the equip
// preview -- HudLayer only ever needs to know which inventory index (if
// any) is currently hovered.
struct InventoryItemHoverChangedMessage
{
    std::optional<int> inventory_index;
};

} // namespace psr
