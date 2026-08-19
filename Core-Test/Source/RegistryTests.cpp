#include "Engine/ECS/Registry.h"

#include <catch2/catch_test_macros.hpp>

namespace {

struct PositionComponent
{
    int x = 0;
    int y = 0;
};

struct TagComponent
{
};

} // namespace

TEST_CASE("Registry creates and destroys entities", "[Registry]")
{
    psr::Registry registry;

    entt::entity entity = registry.CreateEntity();
    REQUIRE(registry.IsValid(entity));

    registry.DestroyEntity(entity);
    REQUIRE_FALSE(registry.IsValid(entity));
}

TEST_CASE("Registry Emplace/GetComponent/HasComponent round-trip", "[Registry]")
{
    psr::Registry registry;
    entt::entity entity = registry.CreateEntity();

    REQUIRE_FALSE(registry.HasComponent<PositionComponent>(entity));

    registry.Emplace<PositionComponent>(entity, 3, 4);

    REQUIRE(registry.HasComponent<PositionComponent>(entity));
    const PositionComponent& position = registry.GetComponent<PositionComponent>(entity);
    REQUIRE(position.x == 3);
    REQUIRE(position.y == 4);

    registry.Remove<PositionComponent>(entity);
    REQUIRE_FALSE(registry.HasComponent<PositionComponent>(entity));
}

TEST_CASE("Registry TryGetComponent returns nullptr when absent", "[Registry]")
{
    psr::Registry registry;
    entt::entity entity = registry.CreateEntity();

    REQUIRE(registry.TryGetComponent<PositionComponent>(entity) == nullptr);

    registry.Emplace<PositionComponent>(entity, 1, 2);
    REQUIRE(registry.TryGetComponent<PositionComponent>(entity) != nullptr);
}

TEST_CASE("Registry Each visits every entity with the component", "[Registry]")
{
    psr::Registry registry;

    entt::entity with_tag_a = registry.CreateEntity();
    entt::entity with_tag_b = registry.CreateEntity();
    entt::entity without_tag = registry.CreateEntity();

    registry.Emplace<TagComponent>(with_tag_a);
    registry.Emplace<TagComponent>(with_tag_b);
    (void)without_tag;

    int visit_count = 0;
    registry.Each<TagComponent>([&visit_count](entt::entity) { ++visit_count; });

    REQUIRE(visit_count == 2);
}
