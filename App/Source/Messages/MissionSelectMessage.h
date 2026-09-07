#pragma once

#include <string>
#include <vector>

namespace psr {

// Resolved Mission Select contents for HudLayer to render -- one row per
// authored Dungeon, fully resolved (no entity handles, no DungeonLibrary
// reference), same "fully resolved" contract CharacterScreenMessage/
// TechniquesScreenMessage already use. Published by MissionSelectState::
// OnEnter (see Missions/MissionSelectSnapshot.h's BuildMissionSelectMessage).
struct MissionSelectMessage
{
    struct Entry
    {
        std::string dungeon_id_string;
        std::string name;
        std::string area_tag;

        // See Missions/RunProgress.h's IsDungeonUnlocked -- a locked row
        // renders dimmed and can't be selected.
        bool unlocked = true;
    };

    std::vector<Entry> entries;
};

} // namespace psr
