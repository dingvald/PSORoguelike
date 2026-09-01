#pragma once

#include "Items/SectionId.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace psr {

// One weighted item-prefab entry in a common/rare pool. section_id_weights is
// sparse in the authored JSON (only overridden IDs are written) but always
// fully populated here -- unlisted IDs default to 1.0 (no favoritism).
struct DropTableEntry
{
    std::uint32_t item_prefab_id = 0;
    float weight = 1.0f;
    std::array<float, kSectionIdCount> section_id_weights{};

    DropTableEntry() { section_id_weights.fill(1.0f); }

    float SectionWeight(SectionId section_id) const
    {
        return weight * section_id_weights[static_cast<std::size_t>(section_id)];
    }
};

// One authored per-enemy/boss drop table: a common pool and a rarer pool (one
// entry rolled from whichever pool wins the rare_roll_chance_percent gate),
// a list of prefabs a boss always drops regardless of that roll, and a
// Meseta range rolled independently of any item. Deliberately minimal --
// one entry picked per kill, no partial/multi-item common-table rolls --
// matching PSO's own "one item slot per table tier" drop shape.
struct DropTable
{
    std::uint32_t id = 0;
    std::string id_string;
    std::string name;
    std::vector<DropTableEntry> common_entries;
    std::vector<DropTableEntry> rare_entries;
    std::vector<std::uint32_t> guaranteed_item_ids;
    float rare_roll_chance_percent = 0.0f;
    int meseta_min = 0;
    int meseta_max = 0;
};

} // namespace psr
