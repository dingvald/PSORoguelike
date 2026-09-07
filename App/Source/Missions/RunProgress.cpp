#include "Missions/RunProgress.h"

#include "Engine/Dungeon/Dungeon.h"

namespace psr {

bool IsDungeonUnlocked(const RunProgress& /*progress*/, const Dungeon& /*dungeon*/)
{
    return true;
}

} // namespace psr
