#include "Actions/WaitAction.h"

#include "Engine/Actions/WaitEvent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/Registry.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("WaitAction always costs kWaitCost with no fallback", "[WaitAction]")
{
    psr::Registry registry;
    psr::Entity actor(registry, registry.CreateEntity());

    psr::WaitAction action;
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::WaitAction::kWaitCost);
    REQUIRE_FALSE(result.fallback);
}

TEST_CASE("WaitAction dispatches BeforeWaitEvent and AfterWaitEvent to the actor", "[WaitAction]")
{
    psr::Registry registry;
    psr::Entity actor(registry, registry.CreateEntity());

    struct WaitProbe
    {
    };
    int before_count = 0;
    int after_count = 0;
    actor.Get<psr::EventHandlerComponent>().Subscribe<psr::BeforeWaitEvent, WaitProbe>(
        [&](psr::Entity, psr::BeforeWaitEvent&) { ++before_count; });
    actor.Get<psr::EventHandlerComponent>().Subscribe<psr::AfterWaitEvent, WaitProbe>(
        [&](psr::Entity, psr::AfterWaitEvent&) { ++after_count; });

    psr::WaitAction action;
    action.Perform(actor);

    REQUIRE(before_count == 1);
    REQUIRE(after_count == 1);
}
