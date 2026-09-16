#include "Components/LastDirectionComponent.h"

#include "Combat/AttackEvent.h"
#include "Engine/Actions/MoveEvent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/Registry.h"

namespace psr {

namespace {
    void ContributeMove(Entity actor, AfterMoveEvent& event)
    {
        actor.Get<LastDirectionComponent>().direction = event.to - event.from;
    }

    void ContributeAttack(Entity actor, BeforeAttackEvent& event)
    {
        // A tile-select ranged attack can dispatch with a zero direction
        // (resolved from SelectedTargetComponent elsewhere) -- keep whichever
        // direction was already known rather than snapping the mag to sit on
        // top of the player.
        if (event.direction != Vec2{})
            actor.Get<LastDirectionComponent>().direction = event.direction;
    }
} // namespace

void LastDirectionComponent::AttachHandlers(entt::registry& registry, entt::entity entity)
{
    Registry& psr_registry = Registry::FromEntt(registry);
    Entity self(psr_registry, entity);
    EventHandlerComponent& events = self.Get<EventHandlerComponent>();

    events.Subscribe<AfterMoveEvent, LastDirectionComponent>([](Entity actor, AfterMoveEvent& event)
                                                              { ContributeMove(actor, event); });
    events.Subscribe<BeforeAttackEvent, LastDirectionComponent>([](Entity actor, BeforeAttackEvent& event)
                                                                 { ContributeAttack(actor, event); });
}

void LastDirectionComponent::DetachHandlers(entt::registry& registry, entt::entity entity)
{
    Registry& psr_registry = Registry::FromEntt(registry);
    Entity self(psr_registry, entity);
    EventHandlerComponent* events = self.TryGet<EventHandlerComponent>();
    if (!events)
        return;

    events->Unsubscribe<AfterMoveEvent, LastDirectionComponent>();
    events->Unsubscribe<BeforeAttackEvent, LastDirectionComponent>();
}

} // namespace psr
