#include "Systems/TweenSystem.h"

#include "Components/TweenComponent.h"
#include "Engine/ECS/Registry.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("UpdateTweens advances the front Tween's elapsed but keeps it queued while still in flight",
          "[TweenSystem]")
{
    psr::Registry registry;
    entt::entity entity = registry.CreateEntity();
    psr::TweenComponent& tween_component = registry.GetOrEmplace<psr::TweenComponent>(entity);
    tween_component.queue.push_back(
        psr::Tween{psr::Vec2f{1.0f, 0.0f}, psr::Vec2f{}, /*duration=*/0.2f, /*elapsed=*/0.0f, nullptr});

    psr::UpdateTweens(registry, 0.1f);

    REQUIRE(registry.HasComponent<psr::TweenComponent>(entity));
    REQUIRE(registry.GetComponent<psr::TweenComponent>(entity).queue.size() == 1);
    REQUIRE(registry.GetComponent<psr::TweenComponent>(entity).queue.front().elapsed == 0.1f);
}

TEST_CASE("UpdateTweens removes the component once the sole queued Tween reaches duration", "[TweenSystem]")
{
    psr::Registry registry;
    entt::entity entity = registry.CreateEntity();
    psr::TweenComponent& tween_component = registry.GetOrEmplace<psr::TweenComponent>(entity);
    tween_component.queue.push_back(
        psr::Tween{psr::Vec2f{1.0f, 0.0f}, psr::Vec2f{}, /*duration=*/0.2f, /*elapsed=*/0.0f, nullptr});

    psr::UpdateTweens(registry, 0.2f);

    REQUIRE_FALSE(registry.HasComponent<psr::TweenComponent>(entity));
}

TEST_CASE("UpdateTweens removes the component when a single update overshoots duration", "[TweenSystem]")
{
    psr::Registry registry;
    entt::entity entity = registry.CreateEntity();
    psr::TweenComponent& tween_component = registry.GetOrEmplace<psr::TweenComponent>(entity);
    tween_component.queue.push_back(
        psr::Tween{psr::Vec2f{1.0f, 0.0f}, psr::Vec2f{}, /*duration=*/0.2f, /*elapsed=*/0.0f, nullptr});

    psr::UpdateTweens(registry, 5.0f);

    REQUIRE_FALSE(registry.HasComponent<psr::TweenComponent>(entity));
}

TEST_CASE("UpdateTweens advances multiple entities' tweens independently", "[TweenSystem]")
{
    psr::Registry registry;
    entt::entity fast = registry.CreateEntity();
    registry.GetOrEmplace<psr::TweenComponent>(fast).queue.push_back(
        psr::Tween{psr::Vec2f{1.0f, 0.0f}, psr::Vec2f{}, /*duration=*/0.1f, /*elapsed=*/0.0f, nullptr});
    entt::entity slow = registry.CreateEntity();
    registry.GetOrEmplace<psr::TweenComponent>(slow).queue.push_back(
        psr::Tween{psr::Vec2f{1.0f, 0.0f}, psr::Vec2f{}, /*duration=*/1.0f, /*elapsed=*/0.0f, nullptr});

    psr::UpdateTweens(registry, 0.1f);

    REQUIRE_FALSE(registry.HasComponent<psr::TweenComponent>(fast));
    REQUIRE(registry.HasComponent<psr::TweenComponent>(slow));
    REQUIRE(registry.GetComponent<psr::TweenComponent>(slow).queue.front().elapsed == 0.1f);
}

TEST_CASE("UpdateTweens processes a queue of Tweens in order, moving on to the next once the first finishes",
          "[TweenSystem]")
{
    psr::Registry registry;
    entt::entity entity = registry.CreateEntity();
    psr::TweenComponent& tween_component = registry.GetOrEmplace<psr::TweenComponent>(entity);
    tween_component.queue.push_back(
        psr::Tween{psr::Vec2f{}, psr::Vec2f{1.0f, 0.0f}, /*duration=*/0.1f, /*elapsed=*/0.0f, nullptr});
    tween_component.queue.push_back(
        psr::Tween{psr::Vec2f{1.0f, 0.0f}, psr::Vec2f{}, /*duration=*/0.1f, /*elapsed=*/0.0f, nullptr});

    // Carries the leftover 0.05s into the second queued Tween within the same
    // call, rather than only ever advancing the front one.
    psr::UpdateTweens(registry, 0.15f);

    REQUIRE(registry.HasComponent<psr::TweenComponent>(entity));
    const psr::TweenComponent& after = registry.GetComponent<psr::TweenComponent>(entity);
    REQUIRE(after.queue.size() == 1);
    REQUIRE(after.queue.front().start_offset == psr::Vec2f{1.0f, 0.0f});
    REQUIRE(after.queue.front().elapsed == 0.05f);
}

TEST_CASE("UpdateTweens fires a finished Tween's on_completion exactly once", "[TweenSystem]")
{
    psr::Registry registry;
    entt::entity entity = registry.CreateEntity();
    int fired = 0;
    psr::TweenComponent& tween_component = registry.GetOrEmplace<psr::TweenComponent>(entity);
    tween_component.queue.push_back(
        psr::Tween{psr::Vec2f{}, psr::Vec2f{1.0f, 0.0f}, /*duration=*/0.1f, /*elapsed=*/0.0f, [&fired]() { ++fired; }});

    psr::UpdateTweens(registry, 0.05f);
    REQUIRE(fired == 0);

    psr::UpdateTweens(registry, 0.05f);
    REQUIRE(fired == 1);

    psr::UpdateTweens(registry, 1.0f); // nothing left queued -- must not fire again
    REQUIRE(fired == 1);
}

TEST_CASE("UpdateTweens cascading through a whole queue in one call fires every completion in order",
          "[TweenSystem]")
{
    psr::Registry registry;
    entt::entity entity = registry.CreateEntity();
    std::vector<int> order;
    psr::TweenComponent& tween_component = registry.GetOrEmplace<psr::TweenComponent>(entity);
    tween_component.queue.push_back(
        psr::Tween{psr::Vec2f{}, psr::Vec2f{1.0f, 0.0f}, 0.1f, 0.0f, [&order]() { order.push_back(1); }});
    tween_component.queue.push_back(
        psr::Tween{psr::Vec2f{1.0f, 0.0f}, psr::Vec2f{}, 0.1f, 0.0f, [&order]() { order.push_back(2); }});

    psr::UpdateTweens(registry, 999.0f);

    REQUIRE_FALSE(registry.HasComponent<psr::TweenComponent>(entity));
    REQUIRE(order == std::vector<int>{1, 2});
}

TEST_CASE("UpdateTweens does not clear a component whose completion queued a follow-up Tween", "[TweenSystem]")
{
    psr::Registry registry;
    entt::entity entity = registry.CreateEntity();
    psr::Registry* registry_ptr = &registry;
    psr::TweenComponent& tween_component = registry.GetOrEmplace<psr::TweenComponent>(entity);
    tween_component.queue.push_back(psr::Tween{psr::Vec2f{}, psr::Vec2f{1.0f, 0.0f}, 0.1f, 0.0f,
                                                [registry_ptr, entity]()
                                                {
                                                    registry_ptr->GetComponent<psr::TweenComponent>(entity)
                                                        .queue.push_back(psr::Tween{psr::Vec2f{1.0f, 0.0f},
                                                                                    psr::Vec2f{}, 0.1f, 0.0f, nullptr});
                                                }});

    psr::UpdateTweens(registry, 0.1f);

    REQUIRE(registry.HasComponent<psr::TweenComponent>(entity));
    REQUIRE(registry.GetComponent<psr::TweenComponent>(entity).queue.size() == 1);
}
