#pragma once

namespace psr {

// Player HP/TP/level snapshot for HudLayer to render bars from. Plain values
// only -- no Entity/Registry -- published by CombatLogBridge whenever the
// player's health or TP changes, and once at construction so the HUD isn't
// blank for a frame. Also (re-)published directly by ExperienceSystem right
// after a level-up, since that's the one player-state change CombatLogBridge
// doesn't itself react to.
struct PlayerStatusMessage
{
    int current_hp = 0;
    int max_hp = 0;

    // has_secondary is false (and the rest of these fields unused) if the
    // player has no TPComponent -- e.g. a class that never spends TP.
    bool has_secondary = false;
    int current_secondary = 0;
    int max_secondary = 0;

    // Defaults to 1 -- matches LevelComponent's own default for a player
    // that hasn't landed a kill yet (ExperienceSystem GetOrEmplace's it lazily).
    int level = 1;
};

} // namespace psr
