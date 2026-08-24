#include "Engine/World/Grid.h"

#include <catch2/catch_test_macros.hpp>

namespace {
constexpr entt::entity kEntityA = entt::entity{1};
constexpr entt::entity kEntityB = entt::entity{2};
constexpr entt::entity kEntityC = entt::entity{3};
} // namespace

TEST_CASE("Grid starts with every cell empty", "[Grid]")
{
    psr::Grid grid{3, 3};

    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}).empty());
}

TEST_CASE("Grid::AddEntity adds an entity to an empty cell", "[Grid]")
{
    psr::Grid grid{3, 3};

    grid.AddEntity(psr::Vec2{1, 1}, kEntityA);

    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}) == std::vector<entt::entity>{kEntityA});
}

TEST_CASE("Grid::AddEntity appends multiple entities to the same cell in insertion order", "[Grid]")
{
    psr::Grid grid{3, 3};

    grid.AddEntity(psr::Vec2{1, 1}, kEntityA);
    grid.AddEntity(psr::Vec2{1, 1}, kEntityB);
    grid.AddEntity(psr::Vec2{1, 1}, kEntityC);

    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}) == std::vector<entt::entity>{kEntityA, kEntityB, kEntityC});
}

TEST_CASE("Grid::AddEntity to one cell does not affect a different cell", "[Grid]")
{
    psr::Grid grid{3, 3};

    grid.AddEntity(psr::Vec2{0, 0}, kEntityA);

    REQUIRE(grid.GetEntities(psr::Vec2{1, 0}).empty());
}

TEST_CASE("Grid::RemoveEntity removes only the given entity, preserving order of the rest", "[Grid]")
{
    psr::Grid grid{3, 3};
    grid.AddEntity(psr::Vec2{1, 1}, kEntityA);
    grid.AddEntity(psr::Vec2{1, 1}, kEntityB);
    grid.AddEntity(psr::Vec2{1, 1}, kEntityC);

    grid.RemoveEntity(psr::Vec2{1, 1}, kEntityB);

    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}) == std::vector<entt::entity>{kEntityA, kEntityC});
}

TEST_CASE("Grid::RemoveEntity of an absent entity is a no-op", "[Grid]")
{
    psr::Grid grid{3, 3};
    grid.AddEntity(psr::Vec2{1, 1}, kEntityA);

    grid.RemoveEntity(psr::Vec2{1, 1}, kEntityB);

    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}) == std::vector<entt::entity>{kEntityA});
}

TEST_CASE("Grid::RemoveEntity removes only one occurrence when an entity was added twice", "[Grid]")
{
    psr::Grid grid{3, 3};
    grid.AddEntity(psr::Vec2{1, 1}, kEntityA);
    grid.AddEntity(psr::Vec2{1, 1}, kEntityA);

    grid.RemoveEntity(psr::Vec2{1, 1}, kEntityA);

    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}) == std::vector<entt::entity>{kEntityA});
}

TEST_CASE("Grid::GetEntities on an out-of-bounds tile returns an empty list", "[Grid]")
{
    psr::Grid grid{3, 3};

    REQUIRE(grid.GetEntities(psr::Vec2{-1, -1}).empty());
    REQUIRE(grid.GetEntities(psr::Vec2{3, 3}).empty());
}
