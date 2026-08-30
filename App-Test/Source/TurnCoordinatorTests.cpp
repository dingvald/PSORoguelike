#include "Systems/TurnCoordinator.h"

#include "Actions/ITargetRequestSink.h"
#include "Actions/WaitAction.h"
#include "Combat/StatusEffect.h"
#include "Combat/StatusEffectApplication.h"
#include "Combat/StatusEffectLibrary.h"
#include "Components/EnergyComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/StatusEffectComponent.h"
#include "Engine/Actions/TurnEvent.h"
#include "Engine/Combat/DeathSystem.h"
#include "Engine/Combat/HealthSystem.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/HealthComponent.h"
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
    psr::StatusEffectLibrary status_effects;
    registry.SetStatusEffectLibrary(status_effects);
    registry.BindComponentEvents<psr::StatusEffectComponent>();
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
    psr::StatusEffectLibrary status_effects;
    registry.SetStatusEffectLibrary(status_effects);
    registry.BindComponentEvents<psr::StatusEffectComponent>();
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
    psr::StatusEffectLibrary status_effects;
    registry.SetStatusEffectLibrary(status_effects);
    registry.BindComponentEvents<psr::StatusEffectComponent>();
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
    psr::StatusEffectLibrary status_effects;
    registry.SetStatusEffectLibrary(status_effects);
    registry.BindComponentEvents<psr::StatusEffectComponent>();
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
    psr::StatusEffectLibrary status_effects;
    registry.SetStatusEffectLibrary(status_effects);
    registry.BindComponentEvents<psr::StatusEffectComponent>();
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

TEST_CASE("TurnCoordinator surfaces TargetingRequested once RequestTargeting is called, bypassing the queue",
          "[TurnCoordinator]")
{
    psr::Registry registry;
    psr::StatusEffectLibrary status_effects;
    registry.SetStatusEffectLibrary(status_effects);
    registry.BindComponentEvents<psr::StatusEffectComponent>();
    psr::TurnCoordinator coordinator(registry);

    entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(player);
    registry.Emplace<psr::EnergyComponent>(player);

    CountingAction cast_action;
    coordinator.RequestTargeting(
        psr::TargetRequest{&cast_action, psr::TargetingMode::Directional, psr::WeaponRangeShape::SingleTarget, 1});

    psr::TurnStep step = coordinator.Step(0.016f);

    REQUIRE(step == psr::TurnStep::TargetingRequested);
    REQUIRE(cast_action.count == 0); // Step() surfaces the request, it doesn't resolve the action itself
}

TEST_CASE("TurnCoordinator TakePendingTargetRequest consumes and clears the pending request", "[TurnCoordinator]")
{
    psr::Registry registry;
    psr::StatusEffectLibrary status_effects;
    registry.SetStatusEffectLibrary(status_effects);
    registry.BindComponentEvents<psr::StatusEffectComponent>();
    psr::TurnCoordinator coordinator(registry);

    CountingAction cast_action;
    const psr::TargetRequest sent{&cast_action, psr::TargetingMode::TargetSquare, psr::WeaponRangeShape::Line, 3};
    coordinator.RequestTargeting(sent);

    psr::TargetRequest taken = coordinator.TakePendingTargetRequest();
    REQUIRE(taken.action == &cast_action);
    REQUIRE(taken.mode == psr::TargetingMode::TargetSquare);
    REQUIRE(taken.shape == psr::WeaponRangeShape::Line);
    REQUIRE(taken.range == 3);

    // Consumed -- a second Step() no longer sees a pending request.
    entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(player);
    registry.Emplace<psr::EnergyComponent>(player);
    psr::TurnStep step = coordinator.Step(0.016f);
    REQUIRE(step == psr::TurnStep::AwaitingInput);
}

TEST_CASE("TurnCoordinator SetPendingAction resolves that action for the player, bypassing ActionMap",
          "[TurnCoordinator]")
{
    psr::Registry registry;
    psr::StatusEffectLibrary status_effects;
    registry.SetStatusEffectLibrary(status_effects);
    registry.BindComponentEvents<psr::StatusEffectComponent>();
    psr::TurnCoordinator coordinator(registry);

    entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(player);
    registry.Emplace<psr::EnergyComponent>(player);

    // No key bound, no key pressed -- only SetPendingAction should let this
    // resolve.
    CountingAction confirmed_action;
    coordinator.SetPendingAction(&confirmed_action);

    psr::TurnStep step = coordinator.Step(0.016f);

    REQUIRE(step == psr::TurnStep::Resolved);
    REQUIRE(confirmed_action.count == 1);
}

TEST_CASE("TurnCoordinator forces a Wait at normal cost when the actor is Frozen, pre-empting any chosen action",
          "[TurnCoordinator]")
{
    psr::Registry registry;
    psr::StatusEffect freeze;
    freeze.id = 1;
    freeze.type = psr::StatusEffectType::Freeze;
    freeze.duration = 3;
    psr::StatusEffectLibrary status_effects{{freeze}};
    registry.SetStatusEffectLibrary(status_effects);
    registry.BindComponentEvents<psr::StatusEffectComponent>();
    psr::TurnCoordinator coordinator(registry);

    entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(player);
    registry.Emplace<psr::EnergyComponent>(player);
    psr::ApplyStatusEffect(psr::Entity(registry, player), status_effects, freeze.id);

    // Even a confirmed pending action must be pre-empted by Freeze -- it
    // stays queued for a later, unfrozen turn instead of running now.
    CountingAction chosen_action;
    coordinator.SetPendingAction(&chosen_action);

    psr::TurnStep step = coordinator.Step(0.016f);

    REQUIRE(step == psr::TurnStep::Resolved);
    REQUIRE(chosen_action.count == 0);
    // TurnQueue's NextActor() fast-forwards a freshly-enqueued actor's energy
    // up to the action threshold before Step() resolves anything -- same
    // post-WaitAction value as "resolves a bound key"'s identical setup, not
    // a plain 0 - kWaitCost.
    REQUIRE(registry.GetComponent<psr::EnergyComponent>(player).energy == 0);
}

TEST_CASE("TurnCoordinator dispatches AfterTurnEvent exactly once per resolved turn", "[TurnCoordinator]")
{
    psr::Registry registry;
    psr::StatusEffectLibrary status_effects;
    registry.SetStatusEffectLibrary(status_effects);
    registry.BindComponentEvents<psr::StatusEffectComponent>();
    psr::TurnCoordinator coordinator(registry);

    entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(player);
    registry.Emplace<psr::EnergyComponent>(player);

    struct TurnProbe
    {
    };
    int turn_events = 0;
    psr::Entity(registry, player)
        .Get<psr::EventHandlerComponent>()
        .Subscribe<psr::AfterTurnEvent, TurnProbe>([&](psr::Entity, psr::AfterTurnEvent&) { ++turn_events; });

    coordinator.KeyBindings().Bind(1, std::make_unique<psr::WaitAction>());
    coordinator.PressKey(1);
    psr::TurnStep step = coordinator.Step(0.016f);

    REQUIRE(step == psr::TurnStep::Resolved);
    REQUIRE(turn_events == 1);
}

TEST_CASE("TurnCoordinator survives a lethal Poison tick destroying the acting entity mid-turn", "[TurnCoordinator]")
{
    psr::Registry registry;
    psr::StatusEffect poison;
    poison.id = 2;
    poison.type = psr::StatusEffectType::Poison;
    poison.magnitude = 999;
    poison.duration = 3;
    psr::StatusEffectLibrary status_effects{{poison}};
    registry.SetStatusEffectLibrary(status_effects);
    registry.BindComponentEvents<psr::StatusEffectComponent>();
    registry.BindSystemEvents<psr::HealthComponent, psr::HealthSystem>();
    registry.BindSystemEvents<psr::HealthComponent, psr::DeathSystem>();
    psr::TurnCoordinator coordinator(registry);

    entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(player);
    registry.Emplace<psr::EnergyComponent>(player);
    psr::HealthComponent health;
    health.current_hp = 5;
    health.max_hp = 5;
    registry.Emplace<psr::HealthComponent>(player, health);
    psr::ApplyStatusEffect(psr::Entity(registry, player), status_effects, poison.id);

    coordinator.KeyBindings().Bind(1, std::make_unique<psr::WaitAction>());
    coordinator.PressKey(1);

    // Must not crash even though this Wait's own AfterTurnEvent tick kills
    // player via the Poison stack applied above.
    psr::TurnStep step = coordinator.Step(0.016f);

    REQUIRE(step == psr::TurnStep::Resolved);
    REQUIRE_FALSE(registry.IsValid(player));
}
