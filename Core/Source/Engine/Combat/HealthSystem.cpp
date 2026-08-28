#include "Engine/Combat/HealthSystem.h"

#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Registry.h"

#include <algorithm>

namespace psr {

namespace {
    void ApplyIncomingDamage(Entity target, IncomingDamageEvent& event)
    {
        HealthComponent& health = target.Get<HealthComponent>();
        health.current_hp = std::clamp(health.current_hp - event.amount, 0, health.max_hp);
        const bool defeated = health.current_hp == 0;

        AfterDamageEvent after{target, event.amount, defeated};
        event.source.Dispatch(after);

        if (defeated)
        {
            DeathEvent death;
            target.Dispatch(death);
        }
    }
} // namespace

void HealthSystem::AttachHandlers(entt::registry& registry, entt::entity entity)
{
    Registry& psr_registry = Registry::FromEntt(registry);
    Entity self(psr_registry, entity);
    self.Get<EventHandlerComponent>().Subscribe<IncomingDamageEvent, HealthSystem>(
        [](Entity target, IncomingDamageEvent& event) { ApplyIncomingDamage(target, event); });
}

void HealthSystem::DetachHandlers(entt::registry& registry, entt::entity entity)
{
    Registry& psr_registry = Registry::FromEntt(registry);
    Entity self(psr_registry, entity);
    self.Get<EventHandlerComponent>().Unsubscribe<IncomingDamageEvent, HealthSystem>();
}

} // namespace psr
