#pragma once

#include <string>

namespace psr {

// Published by HudLayer when the player picks an unlocked row on the
// Mission Select screen; GameplayLayer subscribes, re-validates
// IsDungeonUnlocked, and calls TransitionToWorld(Dungeon, dungeon_id_string).
struct MissionSelectedMessage
{
    std::string dungeon_id_string;
};

} // namespace psr
