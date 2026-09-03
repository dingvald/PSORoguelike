#pragma once

#include "Components/DropTableComponent.h"

#include <cstdint>
#include <random>

namespace psr {

struct DropTableResult
{
    enum class Kind
    {
        None,
        Item,
        Meseta
    };

    Kind kind = Kind::None;
    std::uint32_t item_prefab_id = 0;
    int meseta = 0;
};

// Rolls one kill's worth of loot from table: no_drop_weight, meseta_weight,
// and each entries[i].weight all share one flat weighted pool -- exactly one
// outcome is picked per call, matching PSO's own "one item slot per table
// tier" drop shape. A Meseta outcome's amount is uniform over
// [meseta_min, meseta_max]; an empty/all-zero-weight table always resolves
// to Kind::None. Pure aside from rng -- no Registry/Entity dependency, same
// "pure function, randomness passed in" shape as MaybeApplyElementalStatus.
DropTableResult Roll(const DropTableComponent& table, std::mt19937& rng);

} // namespace psr
