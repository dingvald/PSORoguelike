#include "Missions/AreaProgression.h"

#include "Areas/Area.h"
#include "Areas/AreaLibrary.h"
#include "Engine/Dungeon/Dungeon.h"
#include "Engine/Dungeon/DungeonLibrary.h"

#include <entt/core/hashed_string.hpp>

#include <algorithm>
#include <iterator>

namespace psr {

const Dungeon* NextDungeonInArea(const Dungeon& current, const AreaLibrary& areas, const DungeonLibrary& dungeons)
{
    const Area* area = areas.FindByTag(current.area_tag);
    if (!area)
        return nullptr;

    const auto it = std::find(area->dungeon_id_strings.begin(), area->dungeon_id_strings.end(), current.id_string);
    if (it == area->dungeon_id_strings.end())
        return nullptr;

    const auto next_it = std::next(it);
    if (next_it == area->dungeon_id_strings.end())
        return nullptr;

    return dungeons.Find(entt::hashed_string::value(next_it->c_str()));
}

} // namespace psr
