#include "States/ConfirmState.h"

#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Messages/MessageBus.h"
#include "Messages/ConfirmClosedMessage.h"
#include "Messages/ConfirmMessage.h"

#include <SDL3/SDL_keycode.h>

#include <utility>

namespace psr {

void ConfirmState::Configure(std::string message, ConfirmAction action)
{
    m_message = std::move(message);
    m_action = action;
}

void ConfirmState::OnEnter(GameplayContext& context)
{
    m_close_requested = false;
    context.message_bus.Publish(ConfirmMessage{m_message});
}

void ConfirmState::OnExit(GameplayContext& context) { context.message_bus.Publish(ConfirmClosedMessage{}); }

StateTransition ConfirmState::Update(GameplayContext& /*context*/, float /*delta_time*/)
{
    return m_close_requested ? StateTransition::Pop() : StateTransition::None();
}

bool ConfirmState::HandleEvent(Event& event, GameplayContext& /*context*/)
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
