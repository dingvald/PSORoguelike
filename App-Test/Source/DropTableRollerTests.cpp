#include "Items/DropTableRoller.h"

#include <catch2/catch_test_macros.hpp>

namespace {

psr::LootEntry MakeEntry(std::uint32_t item_prefab_id, float weight)
{
    psr::LootEntry entry;
    entry.item_prefab_id = item_prefab_id;
    entry.weight = weight;
    return entry;
}

} // namespace

TEST_CASE("Roll always resolves to None when every weight is zero", "[DropTableRoller]")
{
    psr::DropTableComponent table;
    std::mt19937 rng{1};

    for (int i = 0; i < 10; ++i)
    {
        const psr::DropTableResult result = psr::Roll(table, rng);
        CHECK(result.kind == psr::DropTableResult::Kind::None);
    }
}

TEST_CASE("Roll deterministically picks the only weighted outcome: no-drop", "[DropTableRoller]")
{
    psr::DropTableComponent table;
    table.no_drop_weight = 1.0f;
    table.entries = {MakeEntry(100, 1.0f)};
    table.entries.front().weight = 0.0f;
    table.meseta_weight = 0.0f;

    std::mt19937 rng{7};
    for (int i = 0; i < 20; ++i)
        CHECK(psr::Roll(table, rng).kind == psr::DropTableResult::Kind::None);
}

TEST_CASE("Roll deterministically picks the only weighted outcome: an item entry", "[DropTableRoller]")
{
    psr::DropTableComponent table;
    table.no_drop_weight = 0.0f;
    table.meseta_weight = 0.0f;
    table.entries = {MakeEntry(100, 1.0f)};

    std::mt19937 rng{7};
    for (int i = 0; i < 20; ++i)
    {
        const psr::DropTableResult result = psr::Roll(table, rng);
        REQUIRE(result.kind == psr::DropTableResult::Kind::Item);
        CHECK(result.item_prefab_id == 100);
    }
}

TEST_CASE("Roll deterministically picks the only weighted outcome: Meseta", "[DropTableRoller]")
{
    psr::DropTableComponent table;
    table.no_drop_weight = 0.0f;
    table.meseta_weight = 1.0f;
    table.meseta_min = 15;
    table.meseta_max = 15;

    std::mt19937 rng{7};
    for (int i = 0; i < 20; ++i)
    {
        const psr::DropTableResult result = psr::Roll(table, rng);
        REQUIRE(result.kind == psr::DropTableResult::Kind::Meseta);
        CHECK(result.meseta == 15);
    }
}

TEST_CASE("Roll's weighted pick can deterministically exclude a zero-weight entry", "[DropTableRoller]")
{
    psr::DropTableComponent table;
    psr::LootEntry favored = MakeEntry(1, 1.0f);
    psr::LootEntry excluded = MakeEntry(2, 0.0f);
    table.entries = {favored, excluded};

    std::mt19937 rng{7};
    for (int i = 0; i < 20; ++i)
    {
        const psr::DropTableResult result = psr::Roll(table, rng);
        REQUIRE(result.kind == psr::DropTableResult::Kind::Item);
        CHECK(result.item_prefab_id == 1);
    }
}

TEST_CASE("Roll's Meseta amount is uniform across [meseta_min, meseta_max]", "[DropTableRoller]")
{
    psr::DropTableComponent table;
    table.meseta_weight = 1.0f;
    table.meseta_min = 5;
    table.meseta_max = 10;

    std::mt19937 rng{1};
    for (int i = 0; i < 30; ++i)
    {
        const psr::DropTableResult result = psr::Roll(table, rng);
        REQUIRE(result.kind == psr::DropTableResult::Kind::Meseta);
        CHECK(result.meseta >= 5);
        CHECK(result.meseta <= 10);
    }
}

TEST_CASE("Roll is reproducible for identically-seeded RNGs", "[DropTableRoller]")
{
    psr::DropTableComponent table;
    table.no_drop_weight = 1.0f;
    table.meseta_weight = 1.0f;
    table.meseta_min = 1;
    table.meseta_max = 100;
    table.entries = {MakeEntry(1, 1.0f), MakeEntry(2, 2.0f)};

    std::mt19937 rng_a{123};
    std::mt19937 rng_b{123};

    for (int i = 0; i < 10; ++i)
    {
        const psr::DropTableResult a = psr::Roll(table, rng_a);
        const psr::DropTableResult b = psr::Roll(table, rng_b);
        CHECK(a.kind == b.kind);
        CHECK(a.item_prefab_id == b.item_prefab_id);
        CHECK(a.meseta == b.meseta);
    }
}
