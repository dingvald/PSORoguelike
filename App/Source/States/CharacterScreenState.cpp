#include "States/CharacterScreenState.h"

#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Messages/MessageBus.h"
#include "Items/CharacterScreenSnapshot.h"
#include "Messages/CharacterScreenClosedMessage.h"
#include "Messages/CharacterScreenMessage.h"

#include <SDL3/SDL_keycode.h>

namespace psr {

CharacterScreenState::CharacterScreenState(const AffixLibrary& affixes) : m_affixes(&affixes) {}

void CharacterScreenState::OnEnter(GameplayContext& context)
{
    m_close_requested = false;
    context.message_bus.Publish(BuildCharacterScreenMessage(context.registry, context.player, *m_affixes));
}

void CharacterScreenState::OnExit(GameplayContext& context)
{
    context.message_bus.Publish(CharacterScreenClosedMessage{});
}

StateTransition CharacterScreenState::Update(GameplayContext& /*context*/, float /*delta_time*/)
{
    return m_close_requested ? StateTransition::Pop() : StateTransition::None();
}

bool CharacterScreenState::HandleEvent(Event& event, GameplayContext& /*context*/)
{
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>(
        [this](KeyPressedEvent& key_event)
        {
            const int key = key_event.GetKeyCode();
            if (key == SDLK_ESCAPE || key == SDLK_C)
            {
                m_close_requested = true;
                return true;
            }
            return false;
        });
    return event.handled;
}

} // namespace psr
