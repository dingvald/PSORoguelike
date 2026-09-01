#include "Systems/EnemyAiSystem.h"

#include "Actions/AttackAction.h"
#include "Actions/MoveAction.h"
#include "Actions/WaitAction.h"
#include "Components/AiComponent.h"
#include "Components/BlocksMovementComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"
#include "Items/AffixLibrary.h"

#include <catch2/catch_test_macros.hpp>

#include <random>

TEST_CASE("EnemyAiSystem steps toward a distant hostile target", "[EnemyAiSystem]")
{
    psr::Registry registry;
    psr::Grid grid(5, 1);
    psr::AffixLibrary affixes;
    std::mt19937 rng{0};
    psr::EnemyAiSystem ai{grid, registry, affixes, rng};

    const entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{psr::Vec2{0, 0}});
    registry.Emplace<psr::AiComponent>(actor, psr::AiComponent{psr::AiBehavior::ChaseAndAttack, 8});
    grid.AddEntity(psr::Vec2{0, 0}, actor);

    const entt::entity target = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(target);
    registry.Emplace<psr::Position>(target, psr::Position{psr::Vec2{3, 0}});
    grid.AddEntity(psr::Vec2{3, 0}, target);

    psr::IAction* action = ai.Decide(psr::Entity(registry, actor));
    REQUIRE(dynamic_cast<psr::MoveAction*>(action) != nullptr);

    const psr::ActionResult result = action->Perform(psr::Entity(registry, actor));

    CHECK(result.cost == psr::MoveAction::kMoveCost);
    CHECK(registry.GetComponent<psr::Position>(actor).tile == psr::Vec2{1, 0});
}

TEST_CASE("EnemyAiSystem's step into an adjacent hostile falls back to an attack", "[EnemyAiSystem]")
{
    psr::Registry registry;
    psr::Grid grid(2, 1);
    psr::AffixLibrary affixes;
    std::mt19937 rng{0};
    psr::EnemyAiSystem ai{grid, registry, affixes, rng};

    const entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{psr::Vec2{0, 0}});
    registry.Emplace<psr::AiComponent>(actor, psr::AiComponent{psr::AiBehavior::ChaseAndAttack, 8});
    grid.AddEntity(psr::Vec2{0, 0}, actor);

    const entt::entity target = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(target);
    registry.Emplace<psr::Position>(target, psr::Position{psr::Vec2{1, 0}});
    registry.Emplace<psr::HealthComponent>(target, psr::HealthComponent{10, 10});
    registry.Emplace<psr::BlocksMovementComponent>(target);
    grid.AddEntity(psr::Vec2{1, 0}, target);

    psr::IAction* action = ai.Decide(psr::Entity(registry, actor));
    REQUIRE(dynamic_cast<psr::MoveAction*>(action) != nullptr);

    const psr::ActionResult result = action->Perform(psr::Entity(registry, actor));

    // Zero-cost move-that-became-a-bump: the fallback is the actual attack.
    CHECK(result.cost == 0);
    REQUIRE(result.fallback != nullptr);
    CHECK(dynamic_cast<psr::AttackAction*>(result.fallback.get()) != nullptr);
    // Never actually stepped onto the target's tile.
    CHECK(registry.GetComponent<psr::Position>(actor).tile == psr::Vec2{0, 0});
}

TEST_CASE("EnemyAiSystem waits when no hostile target is within detection_range", "[EnemyAiSystem]")
{
    psr::Registry registry;
    psr::Grid grid(10, 1);
    psr::AffixLibrary affixes;
    std::mt19937 rng{0};
    psr::EnemyAiSystem ai{grid, registry, affixes, rng};

    const entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{psr::Vec2{0, 0}});
    registry.Emplace<psr::AiComponent>(actor, psr::AiComponent{psr::AiBehavior::ChaseAndAttack, 2});
    grid.AddEntity(psr::Vec2{0, 0}, actor);

    const entt::entity target = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(target);
    registry.Emplace<psr::Position>(target, psr::Position{psr::Vec2{9, 0}}); // well past detection_range
    grid.AddEntity(psr::Vec2{9, 0}, target);

    psr::IAction* action = ai.Decide(psr::Entity(registry, actor));
    REQUIRE(dynamic_cast<psr::WaitAction*>(action) != nullptr);

    action->Perform(psr::Entity(registry, actor));
    CHECK(registry.GetComponent<psr::Position>(actor).tile == psr::Vec2{0, 0});
}

TEST_CASE("EnemyAiSystem waits (does not loop) when both candidate directions are walled off", "[EnemyAiSystem]")
{
    psr::Registry registry;
    psr::Grid grid(3, 3);
    psr::AffixLibrary affixes;
    std::mt19937 rng{0};
    psr::EnemyAiSystem ai{grid, registry, affixes, rng};

    const entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{psr::Vec2{1, 1}});
    registry.Emplace<psr::AiComponent>(actor, psr::AiComponent{psr::AiBehavior::ChaseAndAttack, 8});
    grid.AddEntity(psr::Vec2{1, 1}, actor);

    // Target diagonally beyond (2,2): both single-step candidates toward it
    // from (1,1) are (2,1) and (1,2) -- wall both off (BlocksMovementComponent,
    // no HealthComponent, matching wall.json's own shape).
    const entt::entity target = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(target);
    registry.Emplace<psr::Position>(target, psr::Position{psr::Vec2{2, 2}});
    grid.AddEntity(psr::Vec2{2, 2}, target);

    const entt::entity wall_a = registry.CreateEntity();
    registry.Emplace<psr::BlocksMovementComponent>(wall_a);
    grid.AddEntity(psr::Vec2{2, 1}, wall_a);

    const entt::entity wall_b = registry.CreateEntity();
    registry.Emplace<psr::BlocksMovementComponent>(wall_b);
    grid.AddEntity(psr::Vec2{1, 2}, wall_b);

    psr::IAction* action = ai.Decide(psr::Entity(registry, actor));
    REQUIRE(dynamic_cast<psr::WaitAction*>(action) != nullptr);

    action->Perform(psr::Entity(registry, actor));
    CHECK(registry.GetComponent<psr::Position>(actor).tile == psr::Vec2{1, 1});
}
