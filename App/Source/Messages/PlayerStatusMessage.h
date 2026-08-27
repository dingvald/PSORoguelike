#pragma once

#include <string>

namespace psr {

// Player HP/TP-or-PP snapshot for HudLayer to render bars from. Plain values
// only -- no Entity/Registry -- published by CombatLogBridge whenever the
// player's health or secondary resource changes, and once at construction so
// the HUD isn't blank for a frame.
struct PlayerStatusMessage
{
    int current_hp = 0;
    int max_hp = 0;

    // The player carries at most one of TPComponent (Force) or PPComponent
    // (Hunter/Ranger) -- see docs/GDD.md's PP-vs-TP split. has_secondary is
    // false (and the rest of these fields unused) if the player has neither.
    bool has_secondary = false;
    std::string secondary_label; // "TP" or "PP"
    int current_secondary = 0;
    int max_secondary = 0;
};

} // namespace psr
