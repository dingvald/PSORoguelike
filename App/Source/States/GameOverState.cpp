#include "States/GameOverState.h"

#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Messages/MessageBus.h"
#include "Messages/PlayerDefeatedMessage.h"
#include "Messages/RestartRequestedMessage.h"

namespace psr {

void GameOverState::OnEnter(GameplayContext& context)
{
    m_restart_requested = false;
    context.message_bus.Publish(PlayerDefeatedMessage{});
}

StateTransition GameOverState::Update(GameplayContext& /*context*/, float /*delta_time*/)
{
    return StateTransition::None();
}

bool GameOverState::HandleEvent(Event& event, GameplayContext& context)
{
    // "Press any button to restart" -- the first key press while this state
    // sits on top asks GameplayLayer to rebuild the run (see
    // RestartRequestedMessage; m_restart_requested's own doc comment explains
    // why only the first). Every event is still consumed either way, same as
    // before, so nothing leaks past this state while it's on top of the stack.
    if (!m_restart_requested)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<KeyPressedEvent>(
            [this, &context](KeyPressedEvent&)
            {
                m_restart_requested = true;
                context.message_bus.Publish(RestartRequestedMessage{});
                return true;
            });
    }

    event.handled = true;
    return event.handled;
}

} // namespace psr
