#include "Engine/Combat/StatusEffectApplication.h"

#include "Engine/Combat/StatusEffect.h"
#include "Engine/Combat/StatusEffectLibrary.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/StatusEffectComponent.h"

#include <catch2/catch_test_macros.hpp>

namespace {

psr::StatusEffect MakeEffect(std::uint32_t id, psr::StatusEffectType type, int magnitude, int duration)
{
    psr::StatusEffect effect;
    effect.id = id;
    effect.type = type;
    effect.magnitude = magnitude;
    effect.duration = duration;
    return effect;
}

} // namespace

TEST_CASE("ApplyStatusEffect adds a new stack with duration from the library", "[StatusEffectApplication]")
{
    psr::StatusEffectLibrary library{{MakeEffect(1, psr::StatusEffectType::Poison, /*magnitude=*/3, /*duration=*/4)}};
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity target(registry, handle);

    psr::ApplyStatusEffect(target, library, 1);

    const psr::StatusEffectComponent& status = target.Get<psr::StatusEffectComponent>();
    REQUIRE(status.active.size() == 1);
    CHECK(status.active.front().status_effect_id == 1);
    CHECK(status.active.front().stacks == 1);
    CHECK(status.active.front().remaining_duration == 4);
}

TEST_CASE("ApplyStatusEffect re-applied increments stacks and refreshes duration", "[StatusEffectApplication]")
{
    psr::StatusEffectLibrary library{{MakeEffect(1, psr::StatusEffectType::Poison, /*magnitude=*/3, /*duration=*/4)}};
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity target(registry, handle);

    psr::ApplyStatusEffect(target, library, 1);
    // Simulate the duration having partially ticked down before a second
    // application lands.
    target.Get<psr::StatusEffectComponent>().active.front().remaining_duration = 1;

    psr::ApplyStatusEffect(target, library, 1);

    const psr::StatusEffectComponent& status = target.Get<psr::StatusEffectComponent>();
    REQUIRE(status.active.size() == 1);
    CHECK(status.active.front().stacks == 2);
    CHECK(status.active.front().remaining_duration == 4); // refreshed, not added to the leftover 1
}

TEST_CASE("ApplyStatusEffect no-ops for an id the library doesn't resolve", "[StatusEffectApplication]")
{
    psr::StatusEffectLibrary library; // empty
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity target(registry, handle);

    psr::ApplyStatusEffect(target, library, 999);

    CHECK_FALSE(target.Has<psr::StatusEffectComponent>());
}

TEST_CASE("TickStatusEffects deals Poison damage scaled by stacks and decrements duration", "[StatusEffectApplication]")
{
    psr::StatusEffectLibrary library{{MakeEffect(1, psr::StatusEffectType::Poison, /*magnitude=*/3, /*duration=*/2)}};
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    psr::HealthComponent health;
    health.current_hp = 20;
    health.max_hp = 20;
    actor.Emplace<psr::HealthComponent>(health);

    psr::ApplyStatusEffect(actor, library, 1);
    psr::ApplyStatusEffect(actor, library, 1); // 2 stacks -> 6 damage/tick

    psr::TickStatusEffects(actor, library);

    CHECK(actor.Get<psr::HealthComponent>().current_hp == 14);
    const psr::StatusEffectComponent& status = actor.Get<psr::StatusEffectComponent>();
    REQUIRE(status.active.size() == 1);
    CHECK(status.active.front().remaining_duration == 1);
}

TEST_CASE("TickStatusEffects removes a stack once its duration reaches 0", "[StatusEffectApplication]")
{
    psr::StatusEffectLibrary library{{MakeEffect(1, psr::StatusEffectType::Poison, /*magnitude=*/1, /*duration=*/1)}};
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    psr::HealthComponent health;
    health.current_hp = 20;
    health.max_hp = 20;
    actor.Emplace<psr::HealthComponent>(health);

    psr::ApplyStatusEffect(actor, library, 1);
    psr::TickStatusEffects(actor, library);

    CHECK(actor.Get<psr::StatusEffectComponent>().active.empty());
}

TEST_CASE("TickStatusEffects deals no damage for presence-based types (Freeze/Shock/Confuse)",
          "[StatusEffectApplication]")
{
    psr::StatusEffectLibrary library{
        {MakeEffect(1, psr::StatusEffectType::Freeze, /*magnitude=*/50, /*duration=*/3)}};
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    psr::HealthComponent health;
    health.current_hp = 20;
    health.max_hp = 20;
    actor.Emplace<psr::HealthComponent>(health);

    psr::ApplyStatusEffect(actor, library, 1);
    psr::TickStatusEffects(actor, library);

    CHECK(actor.Get<psr::HealthComponent>().current_hp == 20); // magnitude ignored for Freeze
    CHECK(actor.Get<psr::StatusEffectComponent>().active.front().remaining_duration == 2); // duration still ticks
}

TEST_CASE("TickStatusEffects destroys the entity on a lethal tick without crashing", "[StatusEffectApplication]")
{
    psr::StatusEffectLibrary library{
        {MakeEffect(1, psr::StatusEffectType::Poison, /*magnitude=*/999, /*duration=*/3)}};
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    psr::HealthComponent health;
    health.current_hp = 5;
    health.max_hp = 5;
    actor.Emplace<psr::HealthComponent>(health);

    psr::ApplyStatusEffect(actor, library, 1);
    psr::TickStatusEffects(actor, library); // must not crash

    CHECK_FALSE(registry.IsValid(handle));
}
