#include "Systems/DamageTextSystem.h"

#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Render/FloatingTextSystem.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("DamageTextSystem spawns a white floating number at the target's tile", "[DamageTextSystem]")
{
    psr::Registry registry;
    psr::FloatingTextSystem floating_text;
    psr::DamageTextSystem damage_text(floating_text);

    entt::entity actor_handle = registry.CreateEntity();
    psr::Entity actor(registry, actor_handle);
    damage_text.Subscribe(actor);

    entt::entity target_handle = registry.CreateEntity();
    psr::Entity target(registry, target_handle);
    target.Emplace<psr::Position>(psr::Vec2{5, 7});

    psr::AfterDamageEvent event{target, /*amount=*/12, /*target_defeated=*/false};
    actor.Dispatch(event);

    REQUIRE(floating_text.Active().size() == 1);
    const psr::FloatingTextInstance& instance = floating_text.Active()[0];
    REQUIRE(instance.origin_tile == psr::Vec2{5, 7});
    REQUIRE(instance.text == "12");
    REQUIRE(instance.color == psr::Color{255, 255, 255});
}

TEST_CASE("DamageTextSystem is a no-op when the target has no Position", "[DamageTextSystem]")
{
    psr::Registry registry;
    psr::FloatingTextSystem floating_text;
    psr::DamageTextSystem damage_text(floating_text);

    entt::entity actor_handle = registry.CreateEntity();
    psr::Entity actor(registry, actor_handle);
    damage_text.Subscribe(actor);

    entt::entity target_handle = registry.CreateEntity();
    psr::Entity target(registry, target_handle);

    psr::AfterDamageEvent event{target, /*amount=*/8, /*target_defeated=*/false};
    actor.Dispatch(event);

    REQUIRE(floating_text.Active().empty());
}
