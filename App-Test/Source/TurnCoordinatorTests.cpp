#include "Systems/TurnCoordinator.h"

#include "Actions/WaitAction.h"
#include "Components/EnergyComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Engine/ECS/Registry.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

namespace {

// Counts how many times it was asked to act -- lets tests assert a
// non-player actor actually resolved through the NPC seam, distinguishing
// it from the default WaitAction it stands in for.
class CountingAction : public psr::IAction
{
public:
    psr::ActionResult Perform(psr::Entity /*actor*/) override
    {
        ++count;
        return psr::ActionResult(psr::WaitAction::kWaitCost);
    }

    int count = 0;
};

} // namespace

TEST_CASE("TurnCoordinator yields AwaitingInput when the player has no pending key", "[TurnCoordinator]")
{
    psr::Registry registry;
    psr::TurnCoordinator coordinator(registry);

    entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(player);
    registry.Emplace<psr::EnergyComponent>(player);

    psr::TurnStep step = coordinator.Step(0.016f);

    REQUIRE(step == psr::TurnStep::AwaitingInput);
    REQUIRE(registry.GetComponent<psr::EnergyComponent>(player).energy == 0);
}

TEST_CASE("TurnCoordinator resolves a bound key for the player and returns Resolved", "[TurnCoordinator]")
{
    psr::Registry registry;
    psr::TurnCoordinator coordinator(registry);

    entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(player);
    registry.Emplace<psr::EnergyComponent>(player);

    coordinator.KeyBindings().Bind(1, std::make_unique<psr::WaitAction>());
    coordinator.PressKey(1);

    psr::TurnStep step = coordinator.Step(0.016f);

    REQUIRE(step == psr::TurnStep::Resolved);
    REQUIRE(registry.GetComponent<psr::EnergyComponent>(player).energy == 0);
}

TEST_CASE("TurnCoordinator lets a non-player actor act before applying the player's pending key", "[TurnCoordinator]")
{
    psr::Registry registry;
    psr::TurnCoordinator coordinator(registry);

    CountingAction npc_action;
    coordinator.SetNpcDecision([&npc_action](psr::Entity) -> psr::IAction* { return &npc_action; });

    // Inserted before the player, so it wins the initial energy tie.
    entt::entity npc = registry.CreateEntity();
    registry.Emplace<psr::EnergyComponent>(npc);

    entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(player);
    registry.Emplace<psr::EnergyComponent>(player);

    coordinator.KeyBindings().Bind(1, std::make_unique<psr::WaitAction>());
    coordinator.PressKey(1);

    psr::TurnStep step = coordinator.Step(0.016f);

    REQUIRE(step == psr::TurnStep::Resolved);
    REQUIRE(npc_action.count == 1);
    REQUIRE(registry.GetComponent<psr::EnergyComponent>(player).energy == 0);
}

TEST_CASE("TurnCoordinator lets every ready non-player actor act before yielding when the player has no input",
          "[TurnCoordinator]")
{
    psr::Registry registry;
    psr::TurnCoordinator coordinator(registry);

    CountingAction npc_action;
    coordinator.SetNpcDecision([&npc_action](psr::Entity) -> psr::IAction* { return &npc_action; });

    entt::entity npc_a = registry.CreateEntity();
    registry.Emplace<psr::EnergyComponent>(npc_a);
    entt::entity npc_b = registry.CreateEntity();
    registry.Emplace<psr::EnergyComponent>(npc_b);

    entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(player);
    registry.Emplace<psr::EnergyComponent>(player);

    psr::TurnStep step = coordinator.Step(0.016f);

    REQUIRE(step == psr::TurnStep::AwaitingInput);
    REQUIRE(npc_action.count == 2);
}

TEST_CASE("TurnCoordinator stops tracking an actor once its EnergyComponent is destroyed", "[TurnCoordinator]")
{
    psr::Registry registry;
    psr::TurnCoordinator coordinator(registry);

    entt::entity npc = registry.CreateEntity();
    registry.Emplace<psr::EnergyComponent>(npc);

    entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(player);
    registry.Emplace<psr::EnergyComponent>(player);

    registry.DestroyEntity(npc);

    coordinator.KeyBindings().Bind(1, std::make_unique<psr::WaitAction>());
    coordinator.PressKey(1);

    psr::TurnStep step = coordinator.Step(0.016f);

    REQUIRE(step == psr::TurnStep::Resolved);
}
