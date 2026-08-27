#include "Engine/Items/DropResolution.h"

#include <catch2/catch_test_macros.hpp>

#include <random>

namespace {

psr::DropTable MakeTable()
{
    psr::DropTable table;
    table.common_entries = {{/*item_prefab_id=*/1, /*weight=*/1, psr::SectionId::None}};
    table.rare_entries = {{/*item_prefab_id=*/2, /*weight=*/1, psr::SectionId::None}};
    table.rare_chance_percent = 0;
    return table;
}

} // namespace

TEST_CASE("ResolveDrop rolls meseta uniformly in [meseta_min, meseta_max]", "[DropResolution]")
{
    psr::DropTable table;
    table.meseta_min = 10;
    table.meseta_max = 20;

    std::mt19937 rng{1};
    for (int i = 0; i < 100; ++i)
    {
        const psr::DropResult result = psr::ResolveDrop(table, psr::SectionId::None, rng);
        CHECK(result.meseta >= 10);
        CHECK(result.meseta <= 20);
    }
}

TEST_CASE("ResolveDrop rolls no meseta when meseta_max is 0", "[DropResolution]")
{
    const psr::DropTable table = MakeTable();
    std::mt19937 rng{2};
    CHECK(psr::ResolveDrop(table, psr::SectionId::None, rng).meseta == 0);
}

TEST_CASE("ResolveDrop with rare_chance_percent 0 always picks from the common table", "[DropResolution]")
{
    const psr::DropTable table = MakeTable(); // rare_chance_percent = 0
    std::mt19937 rng{3};
    for (int i = 0; i < 20; ++i)
        CHECK(psr::ResolveDrop(table, psr::SectionId::None, rng).item_prefab_id == 1);
}

TEST_CASE("ResolveDrop with boss_guaranteed_rare always picks from the rare table", "[DropResolution]")
{
    psr::DropTable table = MakeTable();
    table.boss_guaranteed_rare = true;
    std::mt19937 rng{4};
    for (int i = 0; i < 20; ++i)
        CHECK(psr::ResolveDrop(table, psr::SectionId::None, rng).item_prefab_id == 2);
}

TEST_CASE("ResolveDrop falls back to common when the rare roll has no eligible rare entry", "[DropResolution]")
{
    psr::DropTable table;
    table.common_entries = {{/*item_prefab_id=*/1, /*weight=*/1, psr::SectionId::None}};
    table.rare_entries = {{/*item_prefab_id=*/2, /*weight=*/1, psr::SectionId::Viridia}}; // locked to Viridia
    table.boss_guaranteed_rare = true;

    std::mt19937 rng{5};
    // Rolling as a Skyly character: the only rare entry is locked to
    // Viridia, so every roll should fall back to the common entry instead of
    // dropping nothing.
    for (int i = 0; i < 20; ++i)
        CHECK(psr::ResolveDrop(table, psr::SectionId::Skyly, rng).item_prefab_id == 1);
}

TEST_CASE("ResolveDrop excludes an entry favoring a different Section ID", "[DropResolution]")
{
    psr::DropTable table;
    table.rare_entries = {{/*item_prefab_id=*/1, /*weight=*/1, psr::SectionId::Viridia},
                          {/*item_prefab_id=*/2, /*weight=*/1, psr::SectionId::Skyly}};
    table.boss_guaranteed_rare = true;

    std::mt19937 rng{6};
    // Rolling as Skyly: only the Skyly-favored entry (id 2) is ever eligible.
    for (int i = 0; i < 20; ++i)
        CHECK(psr::ResolveDrop(table, psr::SectionId::Skyly, rng).item_prefab_id == 2);
}

TEST_CASE("ResolveDrop drops nothing for an empty table", "[DropResolution]")
{
    const psr::DropTable table;
    std::mt19937 rng{7};
    const psr::DropResult result = psr::ResolveDrop(table, psr::SectionId::None, rng);
    CHECK(result.item_prefab_id == 0);
    CHECK(result.meseta == 0);
}
