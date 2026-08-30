#include "Systems/TurnCoordinator.h"

#include "Combat/StatusEffectQueries.h"
#include "Combat/StatusEffectType.h"
#include "Components/PlayerControlledComponent.h"
#include "Engine/Actions/ActionExecutor.h"
#include "Engine/Actions/TurnEvent.h"
#include "Systems/TweenSystem.h"

namespace psr {

TurnCoordinator::TurnCoordinator(Registry& registry, int action_threshold)
    : m_registry(&registry), m_turn_queue(action_threshold),
      m_decide_npc_action([this](Entity) -> IAction* { return &m_default_npc_action; })
{
    registry.OnConstruct<EnergyComponent, &TurnCoordinator::OnEnergyConstructed>(*this);
    registry.OnDestroy<EnergyComponent, &TurnCoordinator::OnEnergyDestroyed>(*this);
}

TurnCoordinator::~TurnCoordinator() { m_registry->DisconnectComponentLifecycle<EnergyComponent>(*this); }

void TurnCoordinator::OnEnergyConstructed(entt::registry& registry, entt::entity entity)
{
    m_turn_queue.Enqueue(entity, registry.get<EnergyComponent>(entity).energy);
}

void TurnCoordinator::OnEnergyDestroyed(entt::registry& /*registry*/, entt::entity entity)
{
    m_turn_queue.Remove(entity);
}

void TurnCoordinator::PressKey(int key_code) { m_input_buffer.Press(key_code); }

void TurnCoordinator::ReleaseKey(int key_code) { m_input_buffer.Release(key_code); }

TurnStep TurnCoordinator::Step(float delta_time)
{
    if (m_pending_target_request.action)
        return TurnStep::TargetingRequested;

    UpdateTweens(*m_registry, delta_time);

    m_input_buffer.Update(delta_time);
    if (!m_pending_key)
        m_pending_key = m_input_buffer.Pop();

    while (!m_turn_queue.IsEmpty())
    {
        entt::entity actor_handle = m_turn_queue.NextActor();
        Entity actor(*m_registry, actor_handle);
        const bool is_player = actor.Has<PlayerControlledComponent>();

        IAction* action = nullptr;
        if (HasActiveStatusType(actor, m_registry->GetStatusEffectLibrary(), StatusEffectType::Freeze))
        {
            // Frozen pre-empts action selection entirely (skip Move too, not
            // just cancel one action type), so it can't be expressed as a
            // Before<Action>Event cancellation the way Shock is -- see
            // StatusEffectComponent's own doc comment. Always a real,
            // energy-costing Wait, never a zero-cost substitution: a
            // zero-cost turn here would let this same frozen actor come
            // right back up in this same while loop (the NPC branch below
            // never returns early) or, for the player, resolve without any
            // real time passing -- either way the queue would stall on this
            // actor instead of just skipping their turn.
            action = &m_forced_wait_action;
        }
        else if (is_player)
        {
            if (m_pending_action)
            {
                action = std::exchange(m_pending_action, nullptr);
            }
            else
            {
                if (!m_pending_key)
                    return TurnStep::AwaitingInput;

                action = m_key_bindings.Resolve(*m_pending_key);
                m_pending_key.reset();
                if (!action)
                    return TurnStep::AwaitingInput; // unbound key -- nothing to resolve this call
            }
        }
        else
        {
            action = m_decide_npc_action(actor);
        }

        ActionResult result = ResolveAction(*action, actor);

        // AfterTurnEvent drives StatusEffectComponent's TickStatusEffects
        // (duration countdown, Poison/Burn damage) -- a lethal tick, or
        // latently a self-lethal Damage-family Technique/PhotonArt
        // self-target cast inside ResolveAction itself, can destroy actor;
        // OnEnergyDestroyed already clears its TurnQueue membership in that
        // case, so nothing below is safe to touch once that's happened.
        if (actor.IsValid())
        {
            AfterTurnEvent after_turn;
            actor.Dispatch(after_turn);
        }

        if (!actor.IsValid())
        {
            if (is_player)
                return TurnStep::Resolved;
            continue;
        }

        int energy = m_turn_queue.GetEnergy(actor_handle) - result.cost;
        m_turn_queue.Requeue(actor_handle, energy);
        actor.Get<EnergyComponent>().energy = energy;

        if (is_player)
            return TurnStep::Resolved;
    }

    return TurnStep::AwaitingInput;
}

} // namespace psr
