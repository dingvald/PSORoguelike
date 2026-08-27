#pragma once

#include "Engine/Items/DropEntry.h"

#include <cstdint>
#include <string>
#include <vector>

namespace psr {

// One authored drop table (docs/GDD.md's "Itemization & Section IDs": "each
// enemy has a common base table plus a low-probability rare table... bosses
// have their own guaranteed-meaningful table"), referenced from an enemy/
// boss prefab via DropTableComponent::drop_table_id. rare_chance_percent and
// the meseta range are placeholder defaults -- exact drop-rate tuning is
// explicitly deferred to a future balancing pass per docs/GDD.md, the same
// treatment CombatMath.h already gives its own undocumented-in-source
// constants. boss_guaranteed_rare always rolls the rare table instead of the
// chance check, matching a boss's "guaranteed-meaningful" drop.
struct DropTable
{
    std::uint32_t id = 0;
    std::string id_string;
    std::string name;
    std::vector<DropEntry> common_entries;
    std::vector<DropEntry> rare_entries;
    int rare_chance_percent = 1;
    bool boss_guaranteed_rare = false;
    int meseta_min = 0;
    int meseta_max = 0;
};

} // namespace psr
