#include "Components/TPComponent.h"

#include "Combat/PhotonArtCastEvent.h"
#include "Combat/TechniqueCastEvent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/Registry.h"

namespace psr {

namespace {
    void ContributeTechniqueTp(Entity actor, BeforeTechniqueCastEvent& event)
    {
        if (const TPComponent* tp = actor.TryGet<TPComponent>())
        {
            event.has_tp_component = true;
            event.current_tp = tp->current_tp;
        }
    }

    void ContributePhotonArtTp(Entity actor, BeforePhotonArtCastEvent& event)
    {
        if (const TPComponent* tp = actor.TryGet<TPComponent>())
        {
            event.has_tp_component = true;
            event.current_tp = tp->current_tp;
        }
    }
} // namespace

void TPComponent::AttachHandlers(entt::registry& registry, entt::entity entity)
{
    Registry& psr_registry = Registry::FromEntt(registry);
    Entity self(psr_registry, entity);
    EventHandlerComponent& events = self.Get<EventHandlerComponent>();

    events.Subscribe<BeforeTechniqueCastEvent, TPComponent>([](Entity actor, BeforeTechniqueCastEvent& event)
                                                            { ContributeTechniqueTp(actor, event); });
    events.Subscribe<BeforePhotonArtCastEvent, TPComponent>([](Entity actor, BeforePhotonArtCastEvent& event)
                                                            { ContributePhotonArtTp(actor, event); });
}

void TPComponent::DetachHandlers(entt::registry& registry, entt::entity entity)
{
    Registry& psr_registry = Registry::FromEntt(registry);
    Entity self(psr_registry, entity);
    // TryGet, not Get: this fires from on_destroy<TPComponent> during whole-
    // entity destruction, where entt::registry::destroy()'s pool-removal
    // order is registration order, not declaration order -- EventHandlerComponent
    // may already be gone by the time this runs (see EquipmentComponent::
    // DetachHandlers's own doc comment for the full mechanism).
    EventHandlerComponent* events = self.TryGet<EventHandlerComponent>();
    if (!events)
        return;

    events->Unsubscribe<BeforeTechniqueCastEvent, TPComponent>();
    events->Unsubscribe<BeforePhotonArtCastEvent, TPComponent>();
}

} // namespace psr
