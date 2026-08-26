#include "Actions/WaitAction.h"

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
