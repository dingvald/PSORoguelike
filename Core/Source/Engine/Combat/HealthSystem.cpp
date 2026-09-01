#include "Engine/Combat/HealthSystem.h"

#include "Engine/Combat/DamageEvent.h"
#include "Engine/Combat/HealEvent.h"
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

    void ApplyIncomingHeal(Entity target, IncomingHealEvent& event)
    {
        HealthComponent& health = target.Get<HealthComponent>();
        const int before = health.current_hp;
        health.current_hp = std::clamp(health.current_hp + event.amount, 0, health.max_hp);

        AfterHealEvent after{target, health.current_hp - before};
        event.source.Dispatch(after);
    }
} // namespace

void HealthSystem::AttachHandlers(entt::registry& registry, entt::entity entity)
{
    Registry& psr_registry = Registry::FromEntt(registry);
    Entity self(psr_registry, entity);
    EventHandlerComponent& events = self.GetOrEmplace<EventHandlerComponent>();
    events.Subscribe<IncomingDamageEvent, HealthSystem>([](Entity target, IncomingDamageEvent& event)
                                                        { ApplyIncomingDamage(target, event); });
    events.Subscribe<IncomingHealEvent, HealthSystem>([](Entity target, IncomingHealEvent& event)
                                                      { ApplyIncomingHeal(target, event); });
}

void HealthSystem::DetachHandlers(entt::registry& registry, entt::entity entity)
{
    Registry& psr_registry = Registry::FromEntt(registry);
    Entity self(psr_registry, entity);
    EventHandlerComponent& events = self.GetOrEmplace<EventHandlerComponent>();
    events.Unsubscribe<IncomingDamageEvent, HealthSystem>();
    events.Unsubscribe<IncomingHealEvent, HealthSystem>();
}

} // namespace psr
