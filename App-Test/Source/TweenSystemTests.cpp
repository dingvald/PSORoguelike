#include "Systems/TweenSystem.h"

#include "Components/TweenComponent.h"
#include "Engine/ECS/Registry.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("UpdateTweens advances elapsed but keeps the component while still in flight", "[TweenSystem]")
{
    psr::Registry registry;
    entt::entity entity = registry.CreateEntity();
    registry.Emplace<psr::TweenComponent>(entity, psr::Vec2f{1.0f, 0.0f}, /*duration=*/0.2f, /*elapsed=*/0.0f);

    psr::UpdateTweens(registry, 0.1f);

    REQUIRE(registry.HasComponent<psr::TweenComponent>(entity));
    REQUIRE(registry.GetComponent<psr::TweenComponent>(entity).elapsed == 0.1f);
}

TEST_CASE("UpdateTweens removes the component once elapsed reaches duration", "[TweenSystem]")
{
    psr::Registry registry;
    entt::entity entity = registry.CreateEntity();
    registry.Emplace<psr::TweenComponent>(entity, psr::Vec2f{1.0f, 0.0f}, /*duration=*/0.2f, /*elapsed=*/0.0f);

    psr::UpdateTweens(registry, 0.2f);

    REQUIRE_FALSE(registry.HasComponent<psr::TweenComponent>(entity));
}

TEST_CASE("UpdateTweens removes the component when a single update overshoots duration", "[TweenSystem]")
{
    psr::Registry registry;
    entt::entity entity = registry.CreateEntity();
    registry.Emplace<psr::TweenComponent>(entity, psr::Vec2f{1.0f, 0.0f}, /*duration=*/0.2f, /*elapsed=*/0.0f);

    psr::UpdateTweens(registry, 5.0f);

    REQUIRE_FALSE(registry.HasComponent<psr::TweenComponent>(entity));
}

TEST_CASE("UpdateTweens advances multiple entities' tweens independently", "[TweenSystem]")
{
    psr::Registry registry;
    entt::entity fast = registry.CreateEntity();
    registry.Emplace<psr::TweenComponent>(fast, psr::Vec2f{1.0f, 0.0f}, /*duration=*/0.1f, /*elapsed=*/0.0f);
    entt::entity slow = registry.CreateEntity();
    registry.Emplace<psr::TweenComponent>(slow, psr::Vec2f{1.0f, 0.0f}, /*duration=*/1.0f, /*elapsed=*/0.0f);

    psr::UpdateTweens(registry, 0.1f);

    REQUIRE_FALSE(registry.HasComponent<psr::TweenComponent>(fast));
    REQUIRE(registry.HasComponent<psr::TweenComponent>(slow));
    REQUIRE(registry.GetComponent<psr::TweenComponent>(slow).elapsed == 0.1f);
}
