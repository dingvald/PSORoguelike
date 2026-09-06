#include "Missions/RunProgress.h"

#include "Engine/Dungeon/Dungeon.h"

#include <catch2/catch_test_macros.hpp>

using namespace psr;

// Documents today's placeholder policy explicitly (see RunProgress.h's own
// doc comment) -- every authored Dungeon is unlocked unconditionally, since
// no fixed area order (M4.5) or difficulty tiers (M10.2) exist yet. A future
// change to IsDungeonUnlocked's body will visibly need to update this test.
TEST_CASE("IsDungeonUnlocked is unconditionally true regardless of progress", "[RunProgress]")
{
    Dungeon dungeon;
    dungeon.id = 42;
    dungeon.id_string = "test_dungeon";

    RunProgress empty_progress;
    REQUIRE(IsDungeonUnlocked(empty_progress, dungeon));

    RunProgress progress_with_other_completion;
    progress_with_other_completion.completed_dungeon_ids.insert(1);
    REQUIRE(IsDungeonUnlocked(progress_with_other_completion, dungeon));

    RunProgress progress_with_this_completion;
    progress_with_this_completion.completed_dungeon_ids.insert(dungeon.id);
    REQUIRE(IsDungeonUnlocked(progress_with_this_completion, dungeon));
}
