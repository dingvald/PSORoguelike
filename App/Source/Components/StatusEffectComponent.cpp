#include "Components/StatusEffectComponent.h"

#include "Combat/AttackEvent.h"
#include "Combat/PhotonArtCastEvent.h"
#include "Combat/StatusEffectApplication.h"
#include "Combat/StatusEffectQueries.h"
#include "Combat/StatusEffectType.h"
#include "Combat/TechniqueCastEvent.h"
#include "Engine/Actions/MoveEvent.h"
#include "Engine/Actions/TurnEvent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Math/Vec2.h"

#include <array>
#include <random>

namespace psr {

namespace {

    // A static handler bound via entt's on_construct/on_destroy signals can't
    // capture the game's own std::mt19937& (every IAction instead threads
    // that through its constructor) -- this is the one spot in the codebase
    // that needs randomness with no RNG to thread through, so it owns a
    // small self-seeded generator of its own rather than inventing new
    // Registry plumbing for a single, low-stakes "which way did the confused
    // actor stumble" roll.
    std::mt19937& ConfusionRng()
    {
        static std::mt19937 rng{std::random_device{}()};
        return rng;
    }

    Vec2 RandomCardinalDirection()
    {
        static constexpr std::array<Vec2, 4> kCardinalDirections{{{0, -1}, {0, 1}, {-1, 0}, {1, 0}}};
        std::uniform_int_distribution<std::size_t> pick(0, kCardinalDirections.size() - 1);
        return kCardinalDirections[pick(ConfusionRng())];
    }

    void ContributeMove(Entity actor, BeforeMoveEvent& event)
    {
        if (HasActiveStatusType(actor, actor.GetRegistry().GetStatusEffectLibrary(), StatusEffectType::Confuse))
            event.offset = RandomCardinalDirection();
    }

    template <typename TEvent> void CancelIfShocked(Entity actor, TEvent& event)
    {
        if (HasActiveStatusType(actor, actor.GetRegistry().GetStatusEffectLibrary(), StatusEffectType::Shock))
            event.cancelled = true;
    }

    void ContributeTurnTick(Entity actor, AfterTurnEvent& /*event*/)
    {
        TickStatusEffects(actor, actor.GetRegistry().GetStatusEffectLibrary());
    }

} // namespace

void StatusEffectComponent::AttachHandlers(entt::registry& registry, entt::entity entity)
{
    Registry& psr_registry = Registry::FromEntt(registry);
    Entity self(psr_registry, entity);
    EventHandlerComponent& events = self.Get<EventHandlerComponent>();

    events.Subscribe<BeforeMoveEvent, StatusEffectComponent>([](Entity actor, BeforeMoveEvent& event)
                                                             { ContributeMove(actor, event); });
    events.Subscribe<BeforeAttackEvent, StatusEffectComponent>([](Entity actor, BeforeAttackEvent& event)
                                                               { CancelIfShocked(actor, event); });
    events.Subscribe<BeforePhotonArtCastEvent, StatusEffectComponent>([](Entity actor, BeforePhotonArtCastEvent& event)
                                                                      { CancelIfShocked(actor, event); });
    events.Subscribe<BeforeTechniqueCastEvent, StatusEffectComponent>([](Entity actor, BeforeTechniqueCastEvent& event)
                                                                      { CancelIfShocked(actor, event); });
    events.Subscribe<AfterTurnEvent, StatusEffectComponent>([](Entity actor, AfterTurnEvent& event)
                                                            { ContributeTurnTick(actor, event); });
}

void StatusEffectComponent::DetachHandlers(entt::registry& registry, entt::entity entity)
{
    Registry& psr_registry = Registry::FromEntt(registry);
    Entity self(psr_registry, entity);
    EventHandlerComponent& events = self.Get<EventHandlerComponent>();

    events.Unsubscribe<BeforeMoveEvent, StatusEffectComponent>();
    events.Unsubscribe<BeforeAttackEvent, StatusEffectComponent>();
    events.Unsubscribe<BeforePhotonArtCastEvent, StatusEffectComponent>();
    events.Unsubscribe<BeforeTechniqueCastEvent, StatusEffectComponent>();
    events.Unsubscribe<AfterTurnEvent, StatusEffectComponent>();
}

} // namespace psr
