#include "Missions/MissionSelectSnapshot.h"

#include "Engine/Dungeon/Dungeon.h"
#include "Engine/Dungeon/DungeonLibrary.h"
#include "Missions/RunProgress.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace psr;

namespace {

Dungeon MakeDungeon(std::uint32_t id, std::string id_string, std::string name)
{
    Dungeon dungeon;
    dungeon.id = id;
    dungeon.id_string = std::move(id_string);
    dungeon.name = std::move(name);
    dungeon.area_tag = "Forest";
    return dungeon;
}

} // namespace

TEST_CASE("BuildMissionSelectMessage lists every dungeon, unlocked with empty progress", "[MissionSelectSnapshot]")
{
    DungeonLibrary dungeons(std::vector<Dungeon>{MakeDungeon(1, "test_dungeon", "Test Dungeon")});
    RunProgress progress;

    const MissionSelectMessage message = BuildMissionSelectMessage(dungeons, progress);

    REQUIRE(message.entries.size() == 1);
    REQUIRE(message.entries[0].dungeon_id_string == "test_dungeon");
    REQUIRE(message.entries[0].name == "Test Dungeon");
    REQUIRE(message.entries[0].area_tag == "Forest");
    REQUIRE(message.entries[0].unlocked);
}

TEST_CASE("BuildMissionSelectMessage still lists a completed dungeon as unlocked", "[MissionSelectSnapshot]")
{
    DungeonLibrary dungeons(std::vector<Dungeon>{MakeDungeon(1, "test_dungeon", "Test Dungeon")});
    RunProgress progress;
    progress.completed_dungeon_ids.insert(1);

    const MissionSelectMessage message = BuildMissionSelectMessage(dungeons, progress);

    REQUIRE(message.entries.size() == 1);
    REQUIRE(message.entries[0].unlocked);
}

TEST_CASE("BuildMissionSelectMessage is empty for an empty library", "[MissionSelectSnapshot]")
{
    DungeonLibrary dungeons;
    RunProgress progress;

    const MissionSelectMessage message = BuildMissionSelectMessage(dungeons, progress);
    REQUIRE(message.entries.empty());
}
