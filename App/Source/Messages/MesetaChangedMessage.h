#pragma once

namespace psr {

// Published by LootDropSystem whenever it credits the player's
// CurrencyComponent -- HudLayer updates its Meseta counter from current,
// never touching ECS directly, same contract as PlayerStatusMessage.
struct MesetaChangedMessage
{
    int current_meseta = 0;
    int delta = 0;
};

} // namespace psr
