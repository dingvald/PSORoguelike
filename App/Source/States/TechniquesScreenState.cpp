#include "States/TechniquesScreenState.h"

#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Messages/MessageBus.h"
#include "Items/TechniquesScreenSnapshot.h"
#include "Messages/TechniquesScreenClosedMessage.h"
#include "Messages/TechniquesScreenMessage.h"

#include <SDL3/SDL_keycode.h>

namespace psr {

TechniquesScreenState::TechniquesScreenState(const TechniqueLibrary& techniques, const PhotonArtLibrary& photon_arts)
    : m_techniques(&techniques), m_photon_arts(&photon_arts)
{
}

void TechniquesScreenState::OnEnter(GameplayContext& context)
{
    m_close_requested = false;
    context.message_bus.Publish(BuildTechniquesScreenMessage(context.registry, context.player, *m_techniques,
                                                              *m_photon_arts));
}

void TechniquesScreenState::OnExit(GameplayContext& context)
{
    context.message_bus.Publish(TechniquesScreenClosedMessage{});
}

StateTransition TechniquesScreenState::Update(GameplayContext& /*context*/, float /*delta_time*/)
{
    return m_close_requested ? StateTransition::Pop() : StateTransition::None();
}

bool TechniquesScreenState::HandleEvent(Event& event, GameplayContext& /*context*/)
{
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>(
        [this](KeyPressedEvent& key_event)
        {
            const int key = key_event.GetKeyCode();
            if (key == SDLK_ESCAPE || key == SDLK_T)
            {
                m_close_requested = true;
                return true;
            }
            return false;
        });
    return event.handled;
}

} // namespace psr
