#include "Missions/MissionSelectSnapshot.h"

#include "Areas/Area.h"
#include "Areas/AreaLibrary.h"
#include "Engine/Dungeon/Dungeon.h"
#include "Engine/Dungeon/DungeonLibrary.h"
#include "Missions/RunProgress.h"

#include <entt/core/hashed_string.hpp>

#include <unordered_set>

namespace psr {

MissionSelectMessage BuildMissionSelectMessage(const DungeonLibrary& dungeons, const RunProgress& progress,
                                               const AreaLibrary& areas)
{
    MissionSelectMessage message;

    std::unordered_set<std::string> sequenced_dungeon_ids;
    for (const Area& area : areas.All())
        for (const std::string& id_string : area.dungeon_id_strings)
            sequenced_dungeon_ids.insert(id_string);

    for (const Area& area : areas.All())
    {
        if (area.dungeon_id_strings.empty())
            continue;

        const Dungeon* first = dungeons.Find(entt::hashed_string::value(area.dungeon_id_strings.front().c_str()));
        if (!first)
            continue;

        MissionSelectMessage::Entry entry;
        entry.dungeon_id_string = first->id_string;
        entry.name = area.name;
        entry.area_tag = area.tag;
        entry.unlocked = IsDungeonUnlocked(progress, *first, dungeons, areas);
        message.entries.push_back(std::move(entry));
    }

    for (const Dungeon& dungeon : dungeons.All())
    {
        if (sequenced_dungeon_ids.contains(dungeon.id_string))
            continue;

        MissionSelectMessage::Entry entry;
        entry.dungeon_id_string = dungeon.id_string;
        entry.name = dungeon.name;
        entry.area_tag = dungeon.area_tag;
        entry.unlocked = IsDungeonUnlocked(progress, dungeon, dungeons, areas);
        message.entries.push_back(std::move(entry));
    }

    return message;
}

} // namespace psr
