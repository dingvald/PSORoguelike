#include "Missions/MissionSelectSnapshot.h"

#include "Engine/Dungeon/Dungeon.h"
#include "Engine/Dungeon/DungeonLibrary.h"
#include "Missions/RunProgress.h"

namespace psr {

MissionSelectMessage BuildMissionSelectMessage(const DungeonLibrary& dungeons, const RunProgress& progress)
{
    MissionSelectMessage message;
    message.entries.reserve(dungeons.All().size());
    for (const Dungeon& dungeon : dungeons.All())
    {
        MissionSelectMessage::Entry entry;
        entry.dungeon_id_string = dungeon.id_string;
        entry.name = dungeon.name;
        entry.area_tag = dungeon.area_tag;
        entry.unlocked = IsDungeonUnlocked(progress, dungeon);
        message.entries.push_back(std::move(entry));
    }
    return message;
}

} // namespace psr
