#include "Items/DropTableRoller.h"

#include <catch2/catch_test_macros.hpp>

namespace {

psr::DropTableEntry MakeEntry(std::uint32_t item_prefab_id, float weight)
{
    psr::DropTableEntry entry;
    entry.item_prefab_id = item_prefab_id;
    entry.weight = weight;
    return entry;
}

} // namespace

TEST_CASE("Roll always includes every guaranteed_item_ids entry regardless of the roll", "[DropTableRoller]")
{
    psr::DropTable table;
    table.guaranteed_item_ids = {10, 20};
    table.rare_roll_chance_percent = 0.0f;

    std::mt19937 rng{1};
    const psr::DropTableResult result = psr::Roll(table, psr::SectionId::Viridia, rng);

    REQUIRE(result.item_prefab_ids.size() == 2);
    CHECK(result.item_prefab_ids[0] == 10);
    CHECK(result.item_prefab_ids[1] == 20);
}

TEST_CASE("Roll never draws from the rare pool when rare_roll_chance_percent is 0", "[DropTableRoller]")
{
    psr::DropTable table;
    table.rare_roll_chance_percent = 0.0f;
    table.common_entries = {MakeEntry(100, 1.0f)};
    table.rare_entries = {MakeEntry(999, 1.0f)};

    std::mt19937 rng{42};
    for (int i = 0; i < 20; ++i)
    {
        const psr::DropTableResult result = psr::Roll(table, psr::SectionId::Viridia, rng);
        REQUIRE(result.item_prefab_ids.size() == 1);
        CHECK(result.item_prefab_ids[0] == 100);
    }
}

TEST_CASE("Roll always draws from the rare pool when rare_roll_chance_percent is 100", "[DropTableRoller]")
{
    psr::DropTable table;
    table.rare_roll_chance_percent = 100.0f;
    table.common_entries = {MakeEntry(100, 1.0f)};
    table.rare_entries = {MakeEntry(999, 1.0f)};

    std::mt19937 rng{42};
    for (int i = 0; i < 20; ++i)
    {
        const psr::DropTableResult result = psr::Roll(table, psr::SectionId::Viridia, rng);
        REQUIRE(result.item_prefab_ids.size() == 1);
        CHECK(result.item_prefab_ids[0] == 999);
    }
}

TEST_CASE("Roll picks nothing from an empty or zero-weight pool beyond guaranteed drops", "[DropTableRoller]")
{
    psr::DropTable table;
    table.rare_roll_chance_percent = 0.0f;
    table.guaranteed_item_ids = {7};
    // common_entries left empty.

    std::mt19937 rng{5};
    const psr::DropTableResult result = psr::Roll(table, psr::SectionId::Viridia, rng);

    REQUIRE(result.item_prefab_ids.size() == 1);
    CHECK(result.item_prefab_ids[0] == 7);
}

TEST_CASE("Roll's Section ID weight multiplier can deterministically exclude an entry", "[DropTableRoller]")
{
    psr::DropTable table;
    table.rare_roll_chance_percent = 0.0f;

    psr::DropTableEntry favored = MakeEntry(1, 1.0f);
    psr::DropTableEntry excluded = MakeEntry(2, 1.0f);
    // Zeroed for Redria specifically -- for that Section ID, `favored` is the
    // only entry with any weight at all, so it wins with certainty (not just
    // high probability), keeping this test non-flaky.
    excluded.section_id_weights[static_cast<std::size_t>(psr::SectionId::Redria)] = 0.0f;
    table.common_entries = {favored, excluded};

    std::mt19937 rng{7};
    for (int i = 0; i < 20; ++i)
    {
        const psr::DropTableResult result = psr::Roll(table, psr::SectionId::Redria, rng);
        REQUIRE(result.item_prefab_ids.size() == 1);
        CHECK(result.item_prefab_ids[0] == 1);
    }
}

TEST_CASE("Roll's Meseta is uniform across [meseta_min, meseta_max], 0 when meseta_max is 0", "[DropTableRoller]")
{
    psr::DropTable zero_range;
    std::mt19937 rng{1};
    CHECK(psr::Roll(zero_range, psr::SectionId::Viridia, rng).meseta == 0);

    psr::DropTable fixed_range;
    fixed_range.meseta_min = 15;
    fixed_range.meseta_max = 15;
    for (int i = 0; i < 10; ++i)
        CHECK(psr::Roll(fixed_range, psr::SectionId::Viridia, rng).meseta == 15);

    psr::DropTable ranged;
    ranged.meseta_min = 5;
    ranged.meseta_max = 10;
    for (int i = 0; i < 30; ++i)
    {
        const int meseta = psr::Roll(ranged, psr::SectionId::Viridia, rng).meseta;
        CHECK(meseta >= 5);
        CHECK(meseta <= 10);
    }
}

TEST_CASE("Roll is reproducible for identically-seeded RNGs", "[DropTableRoller]")
{
    psr::DropTable table;
    table.rare_roll_chance_percent = 30.0f;
    table.common_entries = {MakeEntry(1, 1.0f), MakeEntry(2, 2.0f)};
    table.rare_entries = {MakeEntry(3, 1.0f)};
    table.meseta_min = 1;
    table.meseta_max = 100;

    std::mt19937 rng_a{123};
    std::mt19937 rng_b{123};

    for (int i = 0; i < 10; ++i)
    {
        const psr::DropTableResult a = psr::Roll(table, psr::SectionId::Skyly, rng_a);
        const psr::DropTableResult b = psr::Roll(table, psr::SectionId::Skyly, rng_b);
        CHECK(a.item_prefab_ids == b.item_prefab_ids);
        CHECK(a.meseta == b.meseta);
    }
}
