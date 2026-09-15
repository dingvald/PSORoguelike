#include "Systems/DamageEffectSystem.h"

#include "Components/DamageFlashComponent.h"
#include "Components/RenderableComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/Registry.h"

#include <catch2/catch_test_macros.hpp>

namespace {

// Must match DamageEffectSystem.cpp's own kDamageFlashDuration/kDamageFlashCount.
constexpr float kFlashDuration = 0.5f;
constexpr float kFlashPeriod = kFlashDuration / 6.0f; // 3 flashes, on+off per flash

} // namespace

TEST_CASE("DamageEffectSystem flashes the struck entity's RenderableComponent white on the first tick after damage",
          "[DamageEffectSystem]")
{
    psr::Registry registry;

    entt::entity actor_handle = registry.CreateEntity();
    psr::Entity actor(registry, actor_handle);
    psr::DamageEffectSystem::Subscribe(actor);

    entt::entity target_handle = registry.CreateEntity();
    psr::Entity target(registry, target_handle);
    psr::RenderableComponent renderable;
    renderable.color_1 = psr::Color(10, 20, 30);
    renderable.color_2 = psr::Color(40, 50, 60);
    target.Emplace<psr::RenderableComponent>(renderable);

    psr::AfterDamageEvent event{target, 5, false, false};
    actor.Dispatch(event);

    REQUIRE(registry.HasComponent<psr::DamageFlashComponent>(target_handle));

    psr::DamageEffectSystem::Update(registry, 0.0f);

    const psr::RenderableComponent& after = registry.GetComponent<psr::RenderableComponent>(target_handle);
    REQUIRE(after.color_1 == psr::Color(255, 255, 255, 255));
    REQUIRE(after.color_2 == psr::Color(255, 255, 255, 255));
}

TEST_CASE("DamageEffectSystem toggles back to the entity's baseline color between flashes", "[DamageEffectSystem]")
{
    psr::Registry registry;

    entt::entity actor_handle = registry.CreateEntity();
    psr::Entity actor(registry, actor_handle);
    psr::DamageEffectSystem::Subscribe(actor);

    entt::entity target_handle = registry.CreateEntity();
    psr::Entity target(registry, target_handle);
    psr::RenderableComponent renderable;
    renderable.color_1 = psr::Color(10, 20, 30);
    renderable.color_2 = psr::Color(40, 50, 60);
    target.Emplace<psr::RenderableComponent>(renderable);

    psr::AfterDamageEvent event{target, 5, false, false};
    actor.Dispatch(event);

    // Lands inside the first "off" half-cycle (between one and two periods).
    psr::DamageEffectSystem::Update(registry, kFlashPeriod * 1.5f);

    const psr::RenderableComponent& after = registry.GetComponent<psr::RenderableComponent>(target_handle);
    REQUIRE(after.color_1 == psr::Color(10, 20, 30));
    REQUIRE(after.color_2 == psr::Color(40, 50, 60));
    REQUIRE(registry.HasComponent<psr::DamageFlashComponent>(target_handle));
}

TEST_CASE("DamageEffectSystem restores the baseline color and removes its component once the flash finishes",
          "[DamageEffectSystem]")
{
    psr::Registry registry;

    entt::entity actor_handle = registry.CreateEntity();
    psr::Entity actor(registry, actor_handle);
    psr::DamageEffectSystem::Subscribe(actor);

    entt::entity target_handle = registry.CreateEntity();
    psr::Entity target(registry, target_handle);
    psr::RenderableComponent renderable;
    renderable.color_1 = psr::Color(10, 20, 30);
    renderable.color_2 = psr::Color(40, 50, 60);
    target.Emplace<psr::RenderableComponent>(renderable);

    psr::AfterDamageEvent event{target, 5, false, false};
    actor.Dispatch(event);

    psr::DamageEffectSystem::Update(registry, kFlashDuration + 0.1f);

    const psr::RenderableComponent& after = registry.GetComponent<psr::RenderableComponent>(target_handle);
    REQUIRE(after.color_1 == psr::Color(10, 20, 30));
    REQUIRE(after.color_2 == psr::Color(40, 50, 60));
    REQUIRE_FALSE(registry.HasComponent<psr::DamageFlashComponent>(target_handle));
}

TEST_CASE("DamageEffectSystem keeps the pre-hit baseline color when the target is re-hit mid-flash",
          "[DamageEffectSystem]")
{
    psr::Registry registry;

    entt::entity actor_handle = registry.CreateEntity();
    psr::Entity actor(registry, actor_handle);
    psr::DamageEffectSystem::Subscribe(actor);

    entt::entity target_handle = registry.CreateEntity();
    psr::Entity target(registry, target_handle);
    psr::RenderableComponent renderable;
    renderable.color_1 = psr::Color(10, 20, 30);
    renderable.color_2 = psr::Color(40, 50, 60);
    target.Emplace<psr::RenderableComponent>(renderable);

    psr::AfterDamageEvent first_hit{target, 5, false, false};
    actor.Dispatch(first_hit);

    // First tick lands in the "on" (white) phase -- the target's own
    // RenderableComponent no longer holds the baseline color at the moment
    // of the second hit below.
    psr::DamageEffectSystem::Update(registry, 0.0f);

    psr::AfterDamageEvent second_hit{target, 3, false, false};
    actor.Dispatch(second_hit);

    // The re-hit above reset elapsed back to 0 -- this small a step is still
    // within the first "on" (white) half-cycle of the restarted flash.
    psr::DamageEffectSystem::Update(registry, kFlashPeriod * 0.5f);
    {
        const psr::RenderableComponent& mid = registry.GetComponent<psr::RenderableComponent>(target_handle);
        REQUIRE(mid.color_1 == psr::Color(255, 255, 255, 255));
    }

    psr::DamageEffectSystem::Update(registry, kFlashDuration + 0.1f);

    const psr::RenderableComponent& after = registry.GetComponent<psr::RenderableComponent>(target_handle);
    REQUIRE(after.color_1 == psr::Color(10, 20, 30));
    REQUIRE(after.color_2 == psr::Color(40, 50, 60));
}

TEST_CASE("DamageEffectSystem is a no-op when the incoming damage amount is not positive", "[DamageEffectSystem]")
{
    psr::Registry registry;

    entt::entity actor_handle = registry.CreateEntity();
    psr::Entity actor(registry, actor_handle);
    psr::DamageEffectSystem::Subscribe(actor);

    entt::entity target_handle = registry.CreateEntity();
    psr::Entity target(registry, target_handle);
    target.Emplace<psr::RenderableComponent>();

    psr::AfterDamageEvent event{target, 0, false, false};
    actor.Dispatch(event);

    REQUIRE_FALSE(registry.HasComponent<psr::DamageFlashComponent>(target_handle));
}

TEST_CASE("DamageEffectSystem is a no-op when the struck entity has no RenderableComponent", "[DamageEffectSystem]")
{
    psr::Registry registry;

    entt::entity actor_handle = registry.CreateEntity();
    psr::Entity actor(registry, actor_handle);
    psr::DamageEffectSystem::Subscribe(actor);

    entt::entity target_handle = registry.CreateEntity();
    psr::Entity target(registry, target_handle);

    psr::AfterDamageEvent event{target, 5, false, false};
    actor.Dispatch(event);

    REQUIRE_FALSE(registry.HasComponent<psr::DamageFlashComponent>(target_handle));
}
