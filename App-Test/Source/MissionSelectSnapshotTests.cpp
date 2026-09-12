#include "Missions/MissionSelectSnapshot.h"

#include "Areas/Area.h"
#include "Areas/AreaLibrary.h"
#include "Engine/Dungeon/Dungeon.h"
#include "Engine/Dungeon/DungeonLibrary.h"
#include "Missions/RunProgress.h"

#include <entt/core/hashed_string.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace psr;

namespace {

// id is hashed from id_string, mirroring DungeonLibraryFile.cpp's real
// loading path -- BuildMissionSelectMessage's Area-row lookup resolves
// Area::dungeon_id_strings entries the same way (DungeonLibrary::Find on a
// hashed id_string), so a test dungeon needs the same id/id_string
// relationship real content has.
Dungeon MakeDungeon(std::string id_string, std::string name)
{
    Dungeon dungeon;
    dungeon.id = entt::hashed_string::value(id_string.c_str());
    dungeon.id_string = std::move(id_string);
    dungeon.name = std::move(name);
    dungeon.area_tag = "Forest";
    return dungeon;
}

} // namespace

TEST_CASE("BuildMissionSelectMessage lists every dungeon, unlocked with empty progress", "[MissionSelectSnapshot]")
{
    DungeonLibrary dungeons(std::vector<Dungeon>{MakeDungeon("test_dungeon", "Test Dungeon")});
    RunProgress progress;
    AreaLibrary areas;

    const MissionSelectMessage message = BuildMissionSelectMessage(dungeons, progress, areas);

    REQUIRE(message.entries.size() == 1);
    REQUIRE(message.entries[0].dungeon_id_string == "test_dungeon");
    REQUIRE(message.entries[0].name == "Test Dungeon");
    REQUIRE(message.entries[0].area_tag == "Forest");
    REQUIRE(message.entries[0].unlocked);
}

TEST_CASE("BuildMissionSelectMessage still lists a completed dungeon as unlocked", "[MissionSelectSnapshot]")
{
    const Dungeon dungeon = MakeDungeon("test_dungeon", "Test Dungeon");
    DungeonLibrary dungeons(std::vector<Dungeon>{dungeon});
    RunProgress progress;
    progress.completed_dungeon_ids.insert(dungeon.id);
    AreaLibrary areas;

    const MissionSelectMessage message = BuildMissionSelectMessage(dungeons, progress, areas);

    REQUIRE(message.entries.size() == 1);
    REQUIRE(message.entries[0].unlocked);
}

TEST_CASE("BuildMissionSelectMessage is empty for an empty library", "[MissionSelectSnapshot]")
{
    DungeonLibrary dungeons;
    RunProgress progress;
    AreaLibrary areas;

    const MissionSelectMessage message = BuildMissionSelectMessage(dungeons, progress, areas);
    REQUIRE(message.entries.empty());
}

TEST_CASE("BuildMissionSelectMessage folds an Area's sequenced dungeons into a single row for its first dungeon",
          "[MissionSelectSnapshot]")
{
    DungeonLibrary dungeons(std::vector<Dungeon>{MakeDungeon("forest_1", "Forest 1"),
                                                 MakeDungeon("forest_2", "Forest 2"),
                                                 MakeDungeon("test_dungeon", "Test Dungeon")});
    RunProgress progress;

    Area forest;
    forest.name = "Forest";
    forest.tag = "Forest";
    forest.dungeon_id_strings = {"forest_1", "forest_2"};
    AreaLibrary areas(std::vector<Area>{forest});

    const MissionSelectMessage message = BuildMissionSelectMessage(dungeons, progress, areas);

    REQUIRE(message.entries.size() == 2);
    REQUIRE(message.entries[0].name == "Forest");
    REQUIRE(message.entries[0].dungeon_id_string == "forest_1");
    REQUIRE(message.entries[1].dungeon_id_string == "test_dungeon");
}
