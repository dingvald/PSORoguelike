#include "States/ExploringState.h"

#include "Actions/ITargetRequestSink.h"
#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "States/AnimationState.h"
#include "States/GameOverState.h"
#include "States/TargetSelectionState.h"
#include "Systems/TurnCoordinator.h"

namespace psr {

ExploringState::ExploringState(TargetSelectionState& target_selection, GameOverState& game_over,
                               AnimationState& animation)
    : m_target_selection(&target_selection), m_game_over(&game_over), m_animation(&animation)
{
}

StateTransition ExploringState::Update(GameplayContext& context, float delta_time)
{
    const TurnStep step = context.turn_coordinator.Step(delta_time);
    switch (step)
    {
    case TurnStep::TargetingRequested:
    {
        TargetRequest request = context.turn_coordinator.TakePendingTargetRequest();
        m_target_selection->Begin(request, context.player);
        return StateTransition::Push(*m_target_selection);
    }
    case TurnStep::PlayerDefeated:
        return StateTransition::Push(*m_game_over);
    case TurnStep::AnimationsPending:
        return StateTransition::Push(*m_animation);
    case TurnStep::AwaitingInput:
    case TurnStep::Resolved:
        return StateTransition::None();
    }
    return StateTransition::None();
}

bool ExploringState::HandleEvent(Event& event, GameplayContext& context)
{
    // KeyReleasedEvent is handled unconditionally by GameplayLayer::OnEvent
    // before the state machine is ever reached -- see its own doc comment.
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>(
        [&context](KeyPressedEvent& key_event)
        {
            context.turn_coordinator.PressKey(key_event.GetKeyCode());
            return true;
        });
    return event.handled;
}

} // namespace psr
