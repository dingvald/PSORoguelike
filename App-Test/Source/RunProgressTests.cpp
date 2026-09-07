#include "Missions/RunProgress.h"

#include "Areas/Area.h"
#include "Areas/AreaLibrary.h"
#include "Engine/Dungeon/Dungeon.h"
#include "Engine/Dungeon/DungeonLibrary.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace psr;

namespace {

Dungeon MakeDungeon(std::uint32_t id, std::string area_tag)
{
    Dungeon dungeon;
    dungeon.id = id;
    dungeon.id_string = "dungeon_" + std::to_string(id);
    dungeon.area_tag = std::move(area_tag);
    return dungeon;
}

} // namespace

// Documents today's fallback policy explicitly (see RunProgress.h's own doc
// comment) -- a dungeon whose area_tag has no matching Area entry stays
// unconditionally unlocked, same as the pre-M4.5 placeholder policy. A
// future change to IsDungeonUnlocked's body will visibly need to update
// this test.
TEST_CASE("IsDungeonUnlocked is unconditionally true with no matching Area authored", "[RunProgress]")
{
    Dungeon dungeon = MakeDungeon(42, "Forest");
    DungeonLibrary dungeons(std::vector<Dungeon>{dungeon});
    AreaLibrary areas;

    RunProgress empty_progress;
    REQUIRE(IsDungeonUnlocked(empty_progress, dungeon, dungeons, areas));

    RunProgress progress_with_other_completion;
    progress_with_other_completion.completed_dungeon_ids.insert(1);
    REQUIRE(IsDungeonUnlocked(progress_with_other_completion, dungeon, dungeons, areas));
}

TEST_CASE("IsDungeonUnlocked is true for an Area with no unlock predecessor", "[RunProgress]")
{
    Dungeon dungeon = MakeDungeon(1, "Forest");
    DungeonLibrary dungeons(std::vector<Dungeon>{dungeon});

    Area forest;
    forest.tag = "Forest";
    AreaLibrary areas(std::vector<Area>{forest});

    RunProgress progress;
    REQUIRE(IsDungeonUnlocked(progress, dungeon, dungeons, areas));
}

TEST_CASE("IsDungeonUnlocked is false until a predecessor-area dungeon is completed", "[RunProgress]")
{
    Dungeon forest_dungeon = MakeDungeon(1, "Forest");
    Dungeon caves_dungeon = MakeDungeon(2, "Caves");
    DungeonLibrary dungeons(std::vector<Dungeon>{forest_dungeon, caves_dungeon});

    Area caves;
    caves.tag = "Caves";
    caves.unlock_predecessor_tag = "Forest";
    AreaLibrary areas(std::vector<Area>{caves});

    RunProgress progress_without_forest;
    REQUIRE_FALSE(IsDungeonUnlocked(progress_without_forest, caves_dungeon, dungeons, areas));

    RunProgress progress_with_unrelated_completion;
    progress_with_unrelated_completion.completed_dungeon_ids.insert(caves_dungeon.id);
    REQUIRE_FALSE(IsDungeonUnlocked(progress_with_unrelated_completion, caves_dungeon, dungeons, areas));

    RunProgress progress_with_forest_completed;
    progress_with_forest_completed.completed_dungeon_ids.insert(forest_dungeon.id);
    REQUIRE(IsDungeonUnlocked(progress_with_forest_completed, caves_dungeon, dungeons, areas));
}
