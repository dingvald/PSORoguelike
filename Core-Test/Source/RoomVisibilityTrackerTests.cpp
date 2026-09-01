#include "Engine/Dungeon/RoomVisibilityTracker.h"

#include <catch2/catch_test_macros.hpp>

using namespace psr;

TEST_CASE("RoomVisibilityTracker starts with every room Hidden", "[RoomVisibilityTracker]")
{
    RoomVisibilityTracker tracker(3);

    CHECK(tracker.GetVisibility(0) == RoomVisibility::Hidden);
    CHECK(tracker.GetVisibility(1) == RoomVisibility::Hidden);
    CHECK(tracker.GetVisibility(std::nullopt) == RoomVisibility::Hidden);
}

TEST_CASE("RoomVisibilityTracker marks the updated room Visible", "[RoomVisibilityTracker]")
{
    RoomVisibilityTracker tracker(3);

    tracker.Update(0u);

    CHECK(tracker.GetVisibility(0) == RoomVisibility::Visible);
    CHECK(tracker.GetVisibility(1) == RoomVisibility::Hidden);
}

TEST_CASE("RoomVisibilityTracker downgrades the previous room to Explored once the player leaves it",
          "[RoomVisibilityTracker]")
{
    RoomVisibilityTracker tracker(3);

    tracker.Update(0u);
    tracker.Update(1u);

    CHECK(tracker.GetVisibility(0) == RoomVisibility::Explored);
    CHECK(tracker.GetVisibility(1) == RoomVisibility::Visible);
    CHECK(tracker.GetVisibility(2) == RoomVisibility::Hidden);
}

TEST_CASE("RoomVisibilityTracker re-Visible's a previously-Explored room on return", "[RoomVisibilityTracker]")
{
    RoomVisibilityTracker tracker(3);

    tracker.Update(0u);
    tracker.Update(1u);
    tracker.Update(0u);

    CHECK(tracker.GetVisibility(0) == RoomVisibility::Visible);
    CHECK(tracker.GetVisibility(1) == RoomVisibility::Explored);
}

TEST_CASE("RoomVisibilityTracker's nullopt current room doesn't leak Visible onto other untagged tiles",
          "[RoomVisibilityTracker]")
{
    RoomVisibilityTracker tracker(3);

    tracker.Update(0u);
    tracker.Update(std::nullopt); // player now standing on an untagged tile

    CHECK(tracker.GetVisibility(std::nullopt) == RoomVisibility::Hidden);
    CHECK(tracker.GetVisibility(0) == RoomVisibility::Explored);
}

TEST_CASE("RoomVisibilityTracker extends Visible to an adjacency-listed neighbor of the current room",
          "[RoomVisibilityTracker]")
{
    // Room 0 adjacent to 1; room 2 unrelated to either.
    RoomVisibilityTracker tracker(3, {{1}, {0}, {}});

    tracker.Update(0u);

    CHECK(tracker.GetVisibility(0) == RoomVisibility::Visible);
    CHECK(tracker.GetVisibility(1) == RoomVisibility::Visible);
    CHECK(tracker.GetVisibility(2) == RoomVisibility::Hidden);
}

TEST_CASE("RoomVisibilityTracker downgrades a former neighbor to Explored, not Hidden, once out of range",
          "[RoomVisibilityTracker]")
{
    // A chain: 0 <-> 1 <-> 2, no direct 0-2 edge.
    RoomVisibilityTracker tracker(3, {{1}, {0, 2}, {1}});

    tracker.Update(0u); // room 1 seen as a neighbor, marked visited
    tracker.Update(2u); // now in room 2; room 1 is still a neighbor, room 0 is not

    CHECK(tracker.GetVisibility(0) == RoomVisibility::Explored); // out of range, but was visited
    CHECK(tracker.GetVisibility(1) == RoomVisibility::Visible);
    CHECK(tracker.GetVisibility(2) == RoomVisibility::Visible);
}

TEST_CASE("RoomVisibilityTracker without adjacency data behaves exactly as before",
          "[RoomVisibilityTracker]")
{
    RoomVisibilityTracker tracker(2); // default adjacency: empty

    tracker.Update(0u);

    CHECK(tracker.GetVisibility(0) == RoomVisibility::Visible);
    CHECK(tracker.GetVisibility(1) == RoomVisibility::Hidden);
}
