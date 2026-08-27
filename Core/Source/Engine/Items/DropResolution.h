#pragma once

#include "Engine/ECS/SectionId.h"
#include "Engine/Items/DropTable.h"

#include <cstdint>
#include <random>

namespace psr {

// What one DropTable roll produced. item_prefab_id is 0 when nothing dropped
// (an empty/all-zero-weight table, or a rare roll that missed with no
// common fallback either); meseta is 0 when the table has no meseta range.
struct DropResult
{
    std::uint32_t item_prefab_id = 0;
    int meseta = 0;
};

// Rolls one drop against table for a roller with roller_section (see
// SectionIdComponent) -- docs/GDD.md's "each enemy has a common base table
// plus a low-probability rare table... weighted by the character's Section
// ID; bosses have their own guaranteed-meaningful table." table.rare_chance_
// percent (or table.boss_guaranteed_rare) decides whether the rare or common
// list is rolled; within that list, an entry whose favored_section_id is
// SectionId::None is always eligible, an entry favoring roller_section gets
// a weight bonus, and an entry favoring a *different* section is excluded
// entirely (PSO gates some rare/board items by Section ID the same way). A
// rare roll with no eligible rare entry falls back to the common list rather
// than dropping nothing. Meseta is always rolled independently of the item
// pick when table.meseta_max > 0.
DropResult ResolveDrop(const DropTable& table, SectionId roller_section, std::mt19937& rng);

} // namespace psr
