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

        AfterDamageEvent after{
            target, event.amount, event.is_critical, defeated, event.hit_effect_prefab_id, event.hit_effect_duration};
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
    // TryGet, not GetOrEmplace: this fires from on_destroy<HealthComponent>
    // during whole-entity destruction, where entt::registry::destroy()'s
    // pool-removal order is registration order, not declaration order --
    // EventHandlerComponent may already be gone by the time this runs.
    // GetOrEmplace would resurrect it mid-destroy(), corrupting that pool's
    // bookkeeping for the rest of the entity's (about to be recycled) index.
    if (EventHandlerComponent* events = self.TryGet<EventHandlerComponent>())
    {
        events->Unsubscribe<IncomingDamageEvent, HealthSystem>();
        events->Unsubscribe<IncomingHealEvent, HealthSystem>();
    }
}

} // namespace psr
