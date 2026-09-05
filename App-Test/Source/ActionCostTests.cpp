#include "Combat/ActionCost.h"

#include "Components/ActorComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/Registry.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("EffectiveMoveCost/EffectiveActCost return the base cost unmodified when ActorComponent is absent",
          "[ActionCost]")
{
    psr::Registry registry;
    psr::Entity actor(registry, registry.CreateEntity());

    CHECK(psr::EffectiveMoveCost(actor, 100) == 100);
    CHECK(psr::EffectiveActCost(actor, 100) == 100);
}

TEST_CASE("EffectiveMoveCost/EffectiveActCost return the base cost unmodified at baseline speed 100", "[ActionCost]")
{
    psr::Registry registry;
    psr::Entity actor(registry, registry.CreateEntity());
    actor.Emplace<psr::ActorComponent>();

    CHECK(psr::EffectiveMoveCost(actor, 100) == 100);
    CHECK(psr::EffectiveActCost(actor, 100) == 100);
}

TEST_CASE("EffectiveMoveCost halves the base cost at double movement_speed", "[ActionCost]")
{
    psr::Registry registry;
    psr::Entity actor(registry, registry.CreateEntity());
    actor.Emplace<psr::ActorComponent>(psr::ActorComponent{0, 200, 100});

    CHECK(psr::EffectiveMoveCost(actor, 100) == 50);
}

TEST_CASE("EffectiveActCost doubles the base cost at half act_speed", "[ActionCost]")
{
    psr::Registry registry;
    psr::Entity actor(registry, registry.CreateEntity());
    actor.Emplace<psr::ActorComponent>(psr::ActorComponent{0, 100, 50});

    CHECK(psr::EffectiveActCost(actor, 100) == 200);
}

TEST_CASE("EffectiveActCost clamps to a minimum cost of 1 at extreme speed", "[ActionCost]")
{
    psr::Registry registry;
    psr::Entity actor(registry, registry.CreateEntity());
    actor.Emplace<psr::ActorComponent>(psr::ActorComponent{0, 100, 10000});

    CHECK(psr::EffectiveActCost(actor, 100) == 1);
}
