#include "Missions/RunProgress.h"

#include "Areas/Area.h"
#include "Areas/AreaLibrary.h"
#include "Engine/Dungeon/Dungeon.h"
#include "Engine/Dungeon/DungeonLibrary.h"

namespace psr {

bool IsDungeonUnlocked(const RunProgress& progress, const Dungeon& dungeon, const DungeonLibrary& dungeons,
                       const AreaLibrary& areas)
{
    const Area* area = areas.FindByTag(dungeon.area_tag);
    if (!area || area->unlock_predecessor_tag.empty())
        return true;

    for (const Dungeon& other : dungeons.All())
        if (other.area_tag == area->unlock_predecessor_tag && progress.completed_dungeon_ids.contains(other.id))
            return true;

    return false;
}

} // namespace psr
