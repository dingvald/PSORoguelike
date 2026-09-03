#include "Components/InnateWeaponComponent.h"

#include "Components/EquipmentComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/Registry.h"

namespace psr {

namespace {
    void DestroyInnateWeapon(Entity target, DeathEvent&)
    {
        const EquipmentComponent* equipment = target.TryGet<EquipmentComponent>();
        if (equipment && equipment->weapon != entt::null)
            target.GetRegistry().DestroyEntity(equipment->weapon);
    }
} // namespace

void InnateWeaponComponent::AttachHandlers(entt::registry& registry, entt::entity entity)
{
    Registry& psr_registry = Registry::FromEntt(registry);
    Entity self(psr_registry, entity);
    // Negative priority: must run before DeathSystem's own DeathEvent handler
    // (default priority 0), which destroys the target -- reading its
    // EquipmentComponent after that would touch an already-destroyed entity.
    self.GetOrEmplace<EventHandlerComponent>().Subscribe<DeathEvent, InnateWeaponComponent>(
        [](Entity target, DeathEvent& event) { DestroyInnateWeapon(target, event); }, -10);
}

void InnateWeaponComponent::DetachHandlers(entt::registry& registry, entt::entity entity)
{
    Registry& psr_registry = Registry::FromEntt(registry);
    Entity self(psr_registry, entity);
    // TryGet, not GetOrEmplace: see EquipmentComponent::DetachHandlers's own
    // doc comment -- this fires from on_destroy<InnateWeaponComponent> mid-
    // destroy(), where EventHandlerComponent may already be gone.
    if (EventHandlerComponent* events = self.TryGet<EventHandlerComponent>())
        events->Unsubscribe<DeathEvent, InnateWeaponComponent>();
}

} // namespace psr
