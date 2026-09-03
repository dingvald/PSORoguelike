#pragma once

#include "Components/HotbarComponent.h"

#include <cstdint>

namespace psr {

// Published by HudLayer once the player, having focused a row on the
// Techniques/Photon Arts screen and pressed Space, presses a number key
// naming the target slot (already resolved to 0-9 by HudLayer, same
// "fully resolved" contract HotbarSlotAssignedMessage uses for items).
// GameplayLayer subscribes and routes to AssignAbilityToHotbarSlot
// (Items/Hotbar.h) -- free/instant, same reasoning as the Character screen's
// own hotbar-assign path.
struct TechniquesScreenSlotAssignedMessage
{
    HotbarSlotType type = HotbarSlotType::Empty; // Technique or PhotonArt
    std::uint32_t id = 0;
    int hotbar_slot = -1;
};

} // namespace psr
