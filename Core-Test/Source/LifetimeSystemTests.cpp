#include "Engine/ECS/LifetimeSystem.h"

#include "Engine/ECS/LifetimeComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"

#include <catch2/catch_test_macros.hpp>

namespace {

struct Fixture
{
    psr::Registry registry;
    psr::Grid grid{3, 3};

    Fixture() { registry.SetGrid(grid); }
};

} // namespace

TEST_CASE("LifetimeSystem::Tick decrements remaining_turns without destroying while still positive",
          "[LifetimeSystem]")
{
    Fixture fixture;
    psr::LifetimeSystem system(fixture.registry);

    const entt::entity entity = fixture.registry.CreateEntity();
    fixture.registry.Emplace<psr::LifetimeComponent>(entity, psr::LifetimeComponent{2});

    system.Tick();

    REQUIRE(fixture.registry.IsValid(entity));
    CHECK(fixture.registry.GetComponent<psr::LifetimeComponent>(entity).remaining_turns == 1);
}

TEST_CASE("LifetimeSystem::Tick destroys an entity once remaining_turns reaches 0", "[LifetimeSystem]")
{
    Fixture fixture;
    psr::LifetimeSystem system(fixture.registry);

    const entt::entity entity = fixture.registry.CreateEntity();
    fixture.registry.Emplace<psr::LifetimeComponent>(entity, psr::LifetimeComponent{1});

    system.Tick();

    REQUIRE_FALSE(fixture.registry.IsValid(entity));
}

TEST_CASE("LifetimeSystem::Tick removes a grid-placed entity from the grid before destroying it",
          "[LifetimeSystem]")
{
    Fixture fixture;
    psr::LifetimeSystem system(fixture.registry);

    const entt::entity entity = fixture.registry.CreateEntity();
    fixture.registry.Emplace<psr::Position>(entity, psr::Position{psr::Vec2{1, 1}});
    fixture.grid.AddEntity(psr::Vec2{1, 1}, entity);
    fixture.registry.Emplace<psr::LifetimeComponent>(entity, psr::LifetimeComponent{1});

    system.Tick();

    REQUIRE_FALSE(fixture.registry.IsValid(entity));
    CHECK(fixture.grid.GetEntities(psr::Vec2{1, 1}).empty());
}

TEST_CASE("LifetimeSystem::Tick destroys an entity with no Position without touching the grid",
          "[LifetimeSystem]")
{
    Fixture fixture;
    psr::LifetimeSystem system(fixture.registry);

    const entt::entity entity = fixture.registry.CreateEntity();
    fixture.registry.Emplace<psr::LifetimeComponent>(entity, psr::LifetimeComponent{1});

    system.Tick();

    REQUIRE_FALSE(fixture.registry.IsValid(entity));
}

TEST_CASE("LifetimeSystem tracks and expires multiple entities independently", "[LifetimeSystem]")
{
    Fixture fixture;
    psr::LifetimeSystem system(fixture.registry);

    const entt::entity short_lived = fixture.registry.CreateEntity();
    fixture.registry.Emplace<psr::LifetimeComponent>(short_lived, psr::LifetimeComponent{1});

    const entt::entity long_lived = fixture.registry.CreateEntity();
    fixture.registry.Emplace<psr::LifetimeComponent>(long_lived, psr::LifetimeComponent{2});

    system.Tick();

    REQUIRE_FALSE(fixture.registry.IsValid(short_lived));
    REQUIRE(fixture.registry.IsValid(long_lived));
    CHECK(fixture.registry.GetComponent<psr::LifetimeComponent>(long_lived).remaining_turns == 1);

    system.Tick();

    REQUIRE_FALSE(fixture.registry.IsValid(long_lived));
}
