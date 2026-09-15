#include "Engine/World/Pathfinder.h"

#include "Engine/World/Grid.h"

#include <catch2/catch_test_macros.hpp>

namespace {
bool AlwaysWalkable(psr::Vec2) { return true; }
} // namespace

TEST_CASE("FindPath returns an empty path when start equals goal", "[Pathfinder]")
{
    psr::Grid grid{5, 5};

    REQUIRE(psr::FindPath(grid, psr::Vec2{2, 2}, psr::Vec2{2, 2}, AlwaysWalkable).empty());
}

TEST_CASE("FindPath returns an empty path when the goal is out of bounds", "[Pathfinder]")
{
    psr::Grid grid{5, 5};

    REQUIRE(psr::FindPath(grid, psr::Vec2{0, 0}, psr::Vec2{5, 5}, AlwaysWalkable).empty());
}

TEST_CASE("FindPath on an open grid excludes start and ends at goal", "[Pathfinder]")
{
    psr::Grid grid{5, 5};

    const std::vector<psr::Vec2> path = psr::FindPath(grid, psr::Vec2{0, 0}, psr::Vec2{3, 0}, AlwaysWalkable);

    REQUIRE(path == std::vector<psr::Vec2>{{1, 0}, {2, 0}, {3, 0}});
}

TEST_CASE("FindPath prefers a diagonal shortcut over a cardinal-only route in open terrain", "[Pathfinder]")
{
    psr::Grid grid{5, 5};

    const std::vector<psr::Vec2> path = psr::FindPath(grid, psr::Vec2{0, 0}, psr::Vec2{3, 3}, AlwaysWalkable);

    REQUIRE(path.size() == static_cast<std::size_t>(psr::ChebyshevDistance(psr::Vec2{0, 0}, psr::Vec2{3, 3})));
}

TEST_CASE("FindPath hugs the straight line instead of zigzagging when dx and dy differ", "[Pathfinder]")
{
    psr::Grid grid{10, 10};

    // (0,0) -> (5,2): the optimal step count is Chebyshev distance (5), which
    // ties for many step orderings (e.g. diagonal-then-straight vs.
    // diagonal/cardinal interleaved unevenly). The canonical, straight-
    // looking path front-loads the 2 diagonal steps then finishes cardinal,
    // so y should move monotonically toward the goal with no backtracking.
    const std::vector<psr::Vec2> path = psr::FindPath(grid, psr::Vec2{0, 0}, psr::Vec2{5, 2}, AlwaysWalkable);

    REQUIRE(path.size() == static_cast<std::size_t>(psr::ChebyshevDistance(psr::Vec2{0, 0}, psr::Vec2{5, 2})));
    int previous_y = 0;
    for (psr::Vec2 tile : path)
    {
        REQUIRE(tile.y >= previous_y);
        previous_y = tile.y;
    }
}

TEST_CASE("FindPath routes around a blocked row rather than failing", "[Pathfinder]")
{
    psr::Grid grid{5, 5};

    // A wall across y==2 with a single gap at x==4 -- the only way from the
    // top half to the bottom half.
    const auto is_walkable = [](psr::Vec2 tile) { return tile.y != 2 || tile.x == 4; };

    const std::vector<psr::Vec2> path = psr::FindPath(grid, psr::Vec2{0, 0}, psr::Vec2{0, 4}, is_walkable);

    REQUIRE_FALSE(path.empty());
    for (psr::Vec2 tile : path)
        REQUIRE(is_walkable(tile));
    REQUIRE(path.back() == psr::Vec2{0, 4});
}

TEST_CASE("FindPath returns an empty path when the goal is fully enclosed", "[Pathfinder]")
{
    psr::Grid grid{5, 5};

    // Every tile in the ring surrounding {2,2} is unwalkable, so the goal is
    // unreachable from outside the ring regardless of the goal tile's own
    // walkability.
    const auto encloses_goal = [](psr::Vec2 tile)
    {
        return !(tile.x >= 1 && tile.x <= 3 && tile.y >= 1 && tile.y <= 3 && tile != psr::Vec2{2, 2});
    };

    REQUIRE(psr::FindPath(grid, psr::Vec2{0, 0}, psr::Vec2{2, 2}, encloses_goal).empty());
}
