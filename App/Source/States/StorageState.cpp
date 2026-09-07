#include "States/StorageState.h"

#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Messages/MessageBus.h"
#include "Items/StorageSnapshot.h"
#include "Messages/StorageClosedMessage.h"
#include "Messages/StorageMessage.h"

#include <SDL3/SDL_keycode.h>

namespace psr {

StorageState::StorageState(const AffixLibrary& affixes) : m_affixes(&affixes) {}

void StorageState::OnEnter(GameplayContext& context)
{
    m_close_requested = false;
    context.message_bus.Publish(BuildStorageMessage(context.registry, context.player, *m_affixes));
}

void StorageState::OnExit(GameplayContext& context)
{
    context.message_bus.Publish(StorageClosedMessage{});
}

StateTransition StorageState::Update(GameplayContext& /*context*/, float /*delta_time*/)
{
    return m_close_requested ? StateTransition::Pop() : StateTransition::None();
}

bool StorageState::HandleEvent(Event& event, GameplayContext& /*context*/)
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
