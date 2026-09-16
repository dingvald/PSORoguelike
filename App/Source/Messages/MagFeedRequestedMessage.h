#pragma once

namespace psr {

// Published by HudLayer once the player, having chosen "Feed" on the
// equipped mag, selects an Inventory row recognized as mag food (see
// CharacterScreenMessage::ItemEntry::is_mag_food and HudLayer's
// mag-food-selection sub-state). GameplayLayer subscribes and calls
// ApplyMagFood (free/instant, same as Equip/Remove -- no turn cost).
struct MagFeedRequestedMessage
{
    int food_inventory_index = -1;
};

} // namespace psr
