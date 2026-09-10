#include "Missions/AreaProgression.h"

#include "Areas/Area.h"
#include "Areas/AreaLibrary.h"
#include "Engine/Dungeon/Dungeon.h"
#include "Engine/Dungeon/DungeonLibrary.h"

#include <catch2/catch_test_macros.hpp>

#include <entt/core/hashed_string.hpp>

#include <vector>

using namespace psr;

namespace {

// id is derived from id_string, same as DungeonLibraryFile's real loading --
// NextDungeonInArea resolves the next entry by hashing its id_string, so a
// dungeon's id must actually match that hash for DungeonLibrary::Find to
// locate it (unlike RunProgressTests.cpp's own MakeDungeon, which never
// exercises that lookup and so can use an arbitrary id).
Dungeon MakeDungeon(std::string id_string, std::string area_tag)
{
    Dungeon dungeon;
    dungeon.id_string = std::move(id_string);
    dungeon.id = entt::hashed_string::value(dungeon.id_string.c_str());
    dungeon.area_tag = std::move(area_tag);
    return dungeon;
}

} // namespace

TEST_CASE("NextDungeonInArea returns nullptr with no matching Area authored", "[AreaProgression]")
{
    Dungeon dungeon = MakeDungeon("forest_1", "Forest");
    DungeonLibrary dungeons(std::vector<Dungeon>{dungeon});
    AreaLibrary areas;

    CHECK(NextDungeonInArea(dungeon, areas, dungeons) == nullptr);
}

TEST_CASE("NextDungeonInArea returns nullptr for an Area with no dungeon sequence authored", "[AreaProgression]")
{
    Dungeon dungeon = MakeDungeon("forest_1", "Forest");
    DungeonLibrary dungeons(std::vector<Dungeon>{dungeon});

    Area forest;
    forest.tag = "Forest";
    AreaLibrary areas(std::vector<Area>{forest});

    CHECK(NextDungeonInArea(dungeon, areas, dungeons) == nullptr);
}

TEST_CASE("NextDungeonInArea returns nullptr for the sequence's last entry", "[AreaProgression]")
{
    Dungeon forest_1 = MakeDungeon("forest_1", "Forest");
    Dungeon forest_2 = MakeDungeon("forest_2", "Forest");
    DungeonLibrary dungeons(std::vector<Dungeon>{forest_1, forest_2});

    Area forest;
    forest.tag = "Forest";
    forest.dungeon_id_strings = {"forest_1", "forest_2"};
    AreaLibrary areas(std::vector<Area>{forest});

    CHECK(NextDungeonInArea(forest_2, areas, dungeons) == nullptr);
}

TEST_CASE("NextDungeonInArea resolves the next dungeon in the sequence", "[AreaProgression]")
{
    Dungeon forest_1 = MakeDungeon("forest_1", "Forest");
    Dungeon forest_2 = MakeDungeon("forest_2", "Forest");
    DungeonLibrary dungeons(std::vector<Dungeon>{forest_1, forest_2});

    Area forest;
    forest.tag = "Forest";
    forest.dungeon_id_strings = {"forest_1", "forest_2"};
    AreaLibrary areas(std::vector<Area>{forest});

    const Dungeon* next = NextDungeonInArea(forest_1, areas, dungeons);
    REQUIRE(next != nullptr);
    CHECK(next->id_string == "forest_2");
}

TEST_CASE("NextDungeonInArea returns nullptr when current isn't in its own Area's sequence", "[AreaProgression]")
{
    Dungeon stray = MakeDungeon("stray", "Forest");
    DungeonLibrary dungeons(std::vector<Dungeon>{stray});

    Area forest;
    forest.tag = "Forest";
    forest.dungeon_id_strings = {"forest_1", "forest_2"};
    AreaLibrary areas(std::vector<Area>{forest});

    CHECK(NextDungeonInArea(stray, areas, dungeons) == nullptr);
}
