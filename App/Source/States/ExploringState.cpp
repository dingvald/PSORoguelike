#include "States/ExploringState.h"

#include "Actions/ITargetRequestSink.h"
#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "States/TargetSelectionState.h"
#include "Systems/TurnCoordinator.h"

namespace psr {

ExploringState::ExploringState(TargetSelectionState& target_selection) : m_target_selection(&target_selection) {}

StateTransition ExploringState::Update(GameplayContext& context, float delta_time)
{
    const TurnStep step = context.turn_coordinator.Step(delta_time);
    if (step != TurnStep::TargetingRequested)
        return StateTransition::None();

    TargetRequest request = context.turn_coordinator.TakePendingTargetRequest();
    m_target_selection->Begin(request, context.player);
    return StateTransition::Push(*m_target_selection);
}

bool ExploringState::HandleEvent(Event& event, GameplayContext& context)
{
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>(
        [&context](KeyPressedEvent& key_event)
        {
            context.turn_coordinator.PressKey(key_event.GetKeyCode());
            return true;
        });
    dispatcher.Dispatch<KeyReleasedEvent>(
        [&context](KeyReleasedEvent& key_event)
        {
            context.turn_coordinator.ReleaseKey(key_event.GetKeyCode());
            return true;
        });
    return event.handled;
}

} // namespace psr
