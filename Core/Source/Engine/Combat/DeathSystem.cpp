#include "Engine/Combat/DeathSystem.h"

#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"

namespace psr {

namespace {
    void HandleDeath(Entity self, DeathEvent& /*event*/)
    {
        Registry& registry = self.GetRegistry();
        if (const Position* position = self.TryGet<Position>())
            registry.GetGrid().RemoveEntity(position->tile, self.Handle());

        registry.DestroyEntity(self.Handle());
    }
} // namespace

void DeathSystem::AttachHandlers(entt::registry& registry, entt::entity entity)
{
    Registry& psr_registry = Registry::FromEntt(registry);
    Entity self(psr_registry, entity);
    self.GetOrEmplace<EventHandlerComponent>().Subscribe<DeathEvent, DeathSystem>([](Entity target, DeathEvent& event)
                                                                                  { HandleDeath(target, event); });
}

void DeathSystem::DetachHandlers(entt::registry& registry, entt::entity entity)
{
    Registry& psr_registry = Registry::FromEntt(registry);
    Entity self(psr_registry, entity);
    self.GetOrEmplace<EventHandlerComponent>().Unsubscribe<DeathEvent, DeathSystem>();
}

} // namespace psr
