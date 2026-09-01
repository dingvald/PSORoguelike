#pragma once

#include "Items/DropTable.h"
#include "Items/SectionId.h"

#include <cstdint>
#include <random>
#include <vector>

namespace psr {

struct DropTableResult
{
    std::vector<std::uint32_t> item_prefab_ids;
    int meseta = 0;
};

// Rolls one kill's worth of loot from table: every guaranteed_item_ids entry
// is always included; then rare_roll_chance_percent gates whether one entry
// is weighted-picked from rare_entries or common_entries (weight x that
// entry's section_id multiplier for section_id); an empty/zero-weight pool
// picks nothing. meseta is rolled independently, uniformly across
// [meseta_min, meseta_max]. Pure aside from rng -- no Registry/Entity
// dependency, same "pure function, randomness passed in" shape as
// MaybeApplyElementalStatus.
DropTableResult Roll(const DropTable& table, SectionId section_id, std::mt19937& rng);

} // namespace psr
