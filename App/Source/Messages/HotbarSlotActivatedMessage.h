#pragma once

namespace psr {

// Published by HudLayer when the player clicks a hotbar slot; GameplayLayer
// subscribes and resolves it via HotbarComponent -- HudLayer only ever needs
// to know which slot index was clicked, nothing about what occupies it.
struct HotbarSlotActivatedMessage
{
    int slot_index = -1;
};

} // namespace psr
