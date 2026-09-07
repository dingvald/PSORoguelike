#include "States/ShopState.h"

#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Messages/MessageBus.h"
#include "Messages/ShopClosedMessage.h"
#include "Messages/ShopMessage.h"
#include "Shop/ShopSnapshot.h"

#include <SDL3/SDL_keycode.h>

namespace psr {

ShopState::ShopState(const ShopStock& stock, const AffixLibrary& affixes) : m_stock(&stock), m_affixes(&affixes) {}

void ShopState::OnEnter(GameplayContext& context)
{
    m_close_requested = false;
    context.message_bus.Publish(BuildShopMessage(context.registry, context.player, *m_stock, *m_affixes));
}

void ShopState::OnExit(GameplayContext& context)
{
    context.message_bus.Publish(ShopClosedMessage{});
}

StateTransition ShopState::Update(GameplayContext& /*context*/, float /*delta_time*/)
{
    return m_close_requested ? StateTransition::Pop() : StateTransition::None();
}

bool ShopState::HandleEvent(Event& event, GameplayContext& /*context*/)
{
    // No dedicated toggle key for this screen (opened by walking onto the
    // shopkeeper and pressing Space) -- only Escape closes it.
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
