#include "States/ActionPaletteState.h"

#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Messages/MessageBus.h"
#include "Items/ActionPaletteSnapshot.h"
#include "Messages/ActionPaletteClosedMessage.h"
#include "Messages/ActionPaletteMessage.h"

#include <SDL3/SDL_keycode.h>

namespace psr {

ActionPaletteState::ActionPaletteState(const TechniqueLibrary& techniques, const PhotonArtLibrary& photon_arts)
    : m_techniques(&techniques), m_photon_arts(&photon_arts)
{
}

void ActionPaletteState::OnEnter(GameplayContext& context)
{
    m_close_requested = false;
    context.message_bus.Publish(
        BuildActionPaletteMessage(context.registry, context.player, *m_techniques, *m_photon_arts));
}

void ActionPaletteState::OnExit(GameplayContext& context)
{
    context.message_bus.Publish(ActionPaletteClosedMessage{});
}

StateTransition ActionPaletteState::Update(GameplayContext& /*context*/, float /*delta_time*/)
{
    return m_close_requested ? StateTransition::Pop() : StateTransition::None();
}

bool ActionPaletteState::HandleEvent(Event& event, GameplayContext& /*context*/)
{
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>(
        [this](KeyPressedEvent& key_event)
        {
            const int key = key_event.GetKeyCode();
            if (key == SDLK_ESCAPE || key == SDLK_P)
            {
                m_close_requested = true;
                return true;
            }
            return false;
        });
    return event.handled;
}

} // namespace psr
