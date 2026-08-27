#pragma once

namespace psr {

// Player HP/TP snapshot for HudLayer to render bars from. Plain values
// only -- no Entity/Registry -- published by CombatLogBridge whenever the
// player's health or TP changes, and once at construction so the HUD isn't
// blank for a frame.
struct PlayerStatusMessage
{
    int current_hp = 0;
    int max_hp = 0;

    // has_secondary is false (and the rest of these fields unused) if the
    // player has no TPComponent -- e.g. a class that never spends TP.
    bool has_secondary = false;
    int current_secondary = 0;
    int max_secondary = 0;
};

} // namespace psr
