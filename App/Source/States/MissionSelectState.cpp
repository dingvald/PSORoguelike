#include "States/MissionSelectState.h"

#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Messages/MessageBus.h"
#include "Messages/MissionSelectClosedMessage.h"
#include "Messages/MissionSelectMessage.h"
#include "Missions/MissionSelectSnapshot.h"

#include <SDL3/SDL_keycode.h>

namespace psr {

MissionSelectState::MissionSelectState(const DungeonLibrary& dungeons, const RunProgress& progress,
                                       const AreaLibrary& areas)
    : m_dungeons(&dungeons), m_progress(&progress), m_areas(&areas)
{
}

void MissionSelectState::OnEnter(GameplayContext& context)
{
    m_close_requested = false;
    context.message_bus.Publish(BuildMissionSelectMessage(*m_dungeons, *m_progress, *m_areas));
}

void MissionSelectState::OnExit(GameplayContext& context)
{
    context.message_bus.Publish(MissionSelectClosedMessage{});
}

StateTransition MissionSelectState::Update(GameplayContext& /*context*/, float /*delta_time*/)
{
    return m_close_requested ? StateTransition::Pop() : StateTransition::None();
}

bool MissionSelectState::HandleEvent(Event& event, GameplayContext& /*context*/)
{
    // Unlike CharacterScreenState/TechniquesScreenState, there's no
    // dedicated toggle key for this screen (it's opened by walking onto the
    // hub's teleprompter and pressing Space, not a letter key) -- only
    // Escape closes it.
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
