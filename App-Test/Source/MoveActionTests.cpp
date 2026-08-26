#include "Actions/MoveAction.h"

#include "Components/BlocksMovementComponent.h"
#include "Components/TweenComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("MoveAction moves an actor to an open tile", "[MoveAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    psr::MoveAction action(grid, psr::Vec2{1, 0});
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::MoveAction::kMoveCost);
    REQUIRE(actor.Get<psr::Position>().tile == psr::Vec2{2, 1});
    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}).empty());
    REQUIRE(grid.GetEntities(psr::Vec2{2, 1}) == std::vector<entt::entity>{handle});
}

TEST_CASE("MoveAction emplaces a TweenComponent starting at the pre-move offset", "[MoveAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    psr::MoveAction action(grid, psr::Vec2{1, 0});
    action.Perform(actor);

    const psr::TweenComponent& tween = actor.Get<psr::TweenComponent>();
    REQUIRE(tween.start_offset == psr::Vec2f{-1.0f, 0.0f});
    REQUIRE(tween.duration == psr::MoveAction::kMoveTweenDuration);
    REQUIRE(tween.elapsed == 0.0f);
}

TEST_CASE("MoveAction targeting an out-of-bounds tile is a free no-op", "[MoveAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{0, 0});
    grid.AddEntity(psr::Vec2{0, 0}, handle);

    psr::MoveAction action(grid, psr::Vec2{-1, 0});
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
    REQUIRE(actor.Get<psr::Position>().tile == psr::Vec2{0, 0});
    REQUIRE(grid.GetEntities(psr::Vec2{0, 0}) == std::vector<entt::entity>{handle});
    REQUIRE_FALSE(actor.Has<psr::TweenComponent>());
}

TEST_CASE("MoveAction blocked by a BlocksMovementComponent occupant is a free no-op", "[MoveAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    entt::entity blocker = registry.CreateEntity();
    registry.Emplace<psr::BlocksMovementComponent>(blocker);
    grid.AddEntity(psr::Vec2{2, 1}, blocker);

    psr::MoveAction action(grid, psr::Vec2{1, 0});
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
    REQUIRE(actor.Get<psr::Position>().tile == psr::Vec2{1, 1});
    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}) == std::vector<entt::entity>{handle});
}

TEST_CASE("MoveAction onto a tile occupied by a non-blocking entity still succeeds", "[MoveAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    entt::entity item = registry.CreateEntity();
    grid.AddEntity(psr::Vec2{2, 1}, item);

    psr::MoveAction action(grid, psr::Vec2{1, 0});
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::MoveAction::kMoveCost);
    REQUIRE(actor.Get<psr::Position>().tile == psr::Vec2{2, 1});
    REQUIRE(grid.GetEntities(psr::Vec2{2, 1}) == std::vector<entt::entity>{item, handle});
}
