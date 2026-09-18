#include "States/PauseState.h"

#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Messages/MessageBus.h"
#include "Messages/PauseMenuClosedMessage.h"
#include "Messages/PauseMenuMessage.h"

#include <SDL3/SDL_keycode.h>

namespace psr {

void PauseState::OnEnter(GameplayContext& context)
{
    m_close_requested = false;
    context.message_bus.Publish(PauseMenuMessage{});
}

void PauseState::OnExit(GameplayContext& context) { context.message_bus.Publish(PauseMenuClosedMessage{}); }

StateTransition PauseState::Update(GameplayContext& /*context*/, float /*delta_time*/)
{
    return m_close_requested ? StateTransition::Pop() : StateTransition::None();
}

bool PauseState::HandleEvent(Event& event, GameplayContext& /*context*/)
{
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>(
        [this](KeyPressedEvent& key_event)
        {
            if (key_event.GetKeyCode() == SDLK_ESCAPE)
            {
                m_close_requested = true;
                return true;
            }
            return false;
        });
    return event.handled;
}

} // namespace psr
