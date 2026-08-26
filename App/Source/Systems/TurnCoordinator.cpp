#include "Systems/TurnCoordinator.h"

#include "Components/PlayerControlledComponent.h"
#include "Engine/Actions/ActionExecutor.h"
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
        if (is_player)
        {
            if (!m_pending_key)
                return TurnStep::AwaitingInput;

            action = m_key_bindings.Resolve(*m_pending_key);
            m_pending_key.reset();
            if (!action)
                return TurnStep::AwaitingInput; // unbound key -- nothing to resolve this call
        }
        else
        {
            action = m_decide_npc_action(actor);
        }

        ActionResult result = ResolveAction(*action, actor);
        int energy = m_turn_queue.GetEnergy(actor_handle) - result.cost;
        m_turn_queue.Requeue(actor_handle, energy);
        actor.Get<EnergyComponent>().energy = energy;

        if (is_player)
            return TurnStep::Resolved;
    }

    return TurnStep::AwaitingInput;
}

} // namespace psr
