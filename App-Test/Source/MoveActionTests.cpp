#include "Actions/AttackAction.h"
#include "Actions/MoveAction.h"

#include "Components/BlocksMovementComponent.h"
#include "Components/EquipmentComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/TweenComponent.h"
#include "Engine/Actions/ActionExecutor.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/StatsComponent.h"
#include "Engine/ECS/WeaponComponent.h"
#include "Engine/World/Grid.h"

#include <catch2/catch_test_macros.hpp>

namespace {
psr::AffixLibrary g_no_affixes;
} // namespace

TEST_CASE("MoveAction moves an actor to an open tile", "[MoveAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    std::mt19937 rng{1};
    psr::MoveAction action(grid, g_no_affixes, psr::Vec2{1, 0}, rng);
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

    std::mt19937 rng{1};
    psr::MoveAction action(grid, g_no_affixes, psr::Vec2{1, 0}, rng);
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

    std::mt19937 rng{1};
    psr::MoveAction action(grid, g_no_affixes, psr::Vec2{-1, 0}, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
    REQUIRE(actor.Get<psr::Position>().tile == psr::Vec2{0, 0});
    REQUIRE(grid.GetEntities(psr::Vec2{0, 0}) == std::vector<entt::entity>{handle});
    REQUIRE_FALSE(actor.Has<psr::TweenComponent>());
}

TEST_CASE("MoveAction blocked by a non-attackable BlocksMovementComponent occupant is a free no-op", "[MoveAction]")
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

    std::mt19937 rng{1};
    psr::MoveAction action(grid, g_no_affixes, psr::Vec2{1, 0}, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
    REQUIRE_FALSE(result.fallback);
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

    std::mt19937 rng{1};
    psr::MoveAction action(grid, g_no_affixes, psr::Vec2{1, 0}, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::MoveAction::kMoveCost);
    REQUIRE(grid.GetEntities(psr::Vec2{2, 1}) == std::vector<entt::entity>{item, handle});
}

TEST_CASE("MoveAction bumping into a hostile attackable occupant falls back to an AttackAction", "[MoveAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);
    actor.Emplace<psr::PlayerControlledComponent>();

    // Bump-to-attack needs a weapon equipped (AttackAction is a free no-op
    // otherwise), consistent with attacking via any other route.
    entt::entity weapon = registry.CreateEntity();
    registry.Emplace<psr::WeaponComponent>(weapon);
    registry.Emplace<psr::StatsComponent>(weapon);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
    psr::StatsComponent& actor_stats = actor.GetOrEmplace<psr::StatsComponent>();
    actor_stats.atp = 80;
    actor_stats.ata = 200;

    entt::entity enemy_handle = registry.CreateEntity();
    psr::Entity enemy(registry, enemy_handle);
    registry.Emplace<psr::BlocksMovementComponent>(enemy_handle);
    psr::HealthComponent enemy_health;
    enemy_health.current_hp = 10;
    enemy_health.max_hp = 10;
    enemy.Emplace<psr::HealthComponent>(enemy_health);
    grid.AddEntity(psr::Vec2{2, 1}, enemy_handle);

    std::mt19937 rng{1};
    psr::MoveAction action(grid, g_no_affixes, psr::Vec2{1, 0}, rng);
    psr::ActionResult result = psr::ResolveAction(action, actor); // runs the AttackAction fallback in the same turn

    // The bump itself is free (cost 0); only the resolved AttackAction's own
    // cost is ever applied -- actor never actually steps onto the enemy's
    // tile.
    REQUIRE(result.cost == psr::AttackAction::kAttackCost);
    REQUIRE(actor.Get<psr::Position>().tile == psr::Vec2{1, 1});
    REQUIRE_FALSE(actor.Has<psr::TweenComponent>());
}
