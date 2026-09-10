#include "Systems/HealTextSystem.h"

#include "Engine/Combat/HealEvent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Render/FloatingTextSystem.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("HealTextSystem spawns a green '+N' floating number at the healed entity's tile", "[HealTextSystem]")
{
    psr::Registry registry;
    psr::FloatingTextSystem floating_text;
    psr::HealTextSystem heal_text(floating_text);

    entt::entity source_handle = registry.CreateEntity();
    psr::Entity source(registry, source_handle);
    heal_text.Subscribe(source);

    entt::entity target_handle = registry.CreateEntity();
    psr::Entity target(registry, target_handle);
    target.Emplace<psr::Position>(psr::Vec2{5, 7});

    psr::AfterHealEvent event{target, /*amount=*/25};
    source.Dispatch(event);

    REQUIRE(floating_text.Active().size() == 1);
    const psr::FloatingTextInstance& instance = floating_text.Active()[0];
    REQUIRE(instance.origin_tile == psr::Vec2{5, 7});
    REQUIRE(instance.text == "+25");
    REQUIRE(instance.color == psr::Color{0, 255, 0});
}

TEST_CASE("HealTextSystem is a no-op when the target has no Position", "[HealTextSystem]")
{
    psr::Registry registry;
    psr::FloatingTextSystem floating_text;
    psr::HealTextSystem heal_text(floating_text);

    entt::entity source_handle = registry.CreateEntity();
    psr::Entity source(registry, source_handle);
    heal_text.Subscribe(source);

    entt::entity target_handle = registry.CreateEntity();
    psr::Entity target(registry, target_handle);

    psr::AfterHealEvent event{target, /*amount=*/10};
    source.Dispatch(event);

    REQUIRE(floating_text.Active().empty());
}

TEST_CASE("HealTextSystem is a no-op when the actually-applied amount is zero", "[HealTextSystem]")
{
    psr::Registry registry;
    psr::FloatingTextSystem floating_text;
    psr::HealTextSystem heal_text(floating_text);

    entt::entity source_handle = registry.CreateEntity();
    psr::Entity source(registry, source_handle);
    heal_text.Subscribe(source);

    entt::entity target_handle = registry.CreateEntity();
    psr::Entity target(registry, target_handle);
    target.Emplace<psr::Position>(psr::Vec2{2, 2});

    psr::AfterHealEvent event{target, /*amount=*/0}; // e.g. an item used at full HP
    source.Dispatch(event);

    REQUIRE(floating_text.Active().empty());
}
