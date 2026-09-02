#include "States/CharacterScreenState.h"

#include "Combat/LevelingConfig.h"
#include "Components/InventoryComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Messages/MessageBus.h"
#include "Items/CharacterScreenSnapshot.h"
#include "Items/Equip.h"
#include "Messages/CharacterScreenClosedMessage.h"
#include "Messages/CharacterScreenMessage.h"

#include <SDL3/SDL_keycode.h>

namespace psr {

namespace {
    constexpr int kEquipmentSlotCount = 5;
} // namespace

CharacterScreenState::CharacterScreenState(const AffixLibrary& affixes, const LevelingConfig& leveling)
    : m_affixes(&affixes), m_leveling(&leveling)
{
}

void CharacterScreenState::OnEnter(GameplayContext& context)
{
    m_close_requested = false;
    m_focused_index = 0;
    PublishSnapshot(context);
}

void CharacterScreenState::OnExit(GameplayContext& context)
{
    context.message_bus.Publish(CharacterScreenClosedMessage{});
}

StateTransition CharacterScreenState::Update(GameplayContext& /*context*/, float /*delta_time*/)
{
    return m_close_requested ? StateTransition::Pop() : StateTransition::None();
}

bool CharacterScreenState::HandleEvent(Event& event, GameplayContext& context)
{
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>(
        [this, &context](KeyPressedEvent& key_event)
        {
            const int key = key_event.GetKeyCode();
            switch (key)
            {
            case SDLK_ESCAPE:
            case SDLK_C:
                m_close_requested = true;
                return true;
            case SDLK_UP:
            case SDLK_W:
            case SDLK_KP_8:
                MoveFocus(context, -1);
                return true;
            case SDLK_DOWN:
            case SDLK_S:
            case SDLK_KP_2:
                MoveFocus(context, 1);
                return true;
            case SDLK_SPACE:
                ActivateFocused(context);
                return true;
            default:
                return false;
            }
        });
    return event.handled;
}

int CharacterScreenState::RowCount(GameplayContext& context) const
{
    int inventory_count = 0;
    if (const InventoryComponent* inventory = context.registry.TryGetComponent<InventoryComponent>(context.player))
        inventory_count = static_cast<int>(inventory->items.size());
    return kEquipmentSlotCount + inventory_count;
}

CharacterScreenFocus CharacterScreenState::CurrentFocus() const
{
    CharacterScreenFocus focus;
    if (m_focused_index < kEquipmentSlotCount)
        focus.equipment_slot = static_cast<EquipmentSlot>(m_focused_index);
    else
        focus.inventory_index = m_focused_index - kEquipmentSlotCount;
    return focus;
}

void CharacterScreenState::MoveFocus(GameplayContext& context, int delta)
{
    const int total = RowCount(context);
    if (total <= 0)
        return;
    m_focused_index = ((m_focused_index + delta) % total + total) % total;
    PublishSnapshot(context);
}

void CharacterScreenState::ActivateFocused(GameplayContext& context)
{
    const CharacterScreenFocus focus = CurrentFocus();
    Entity actor(context.registry, context.player);

    bool changed = false;
    if (focus.equipment_slot)
        changed = UnequipSlot(actor, *focus.equipment_slot);
    else if (focus.inventory_index)
        changed = EquipItem(actor, *focus.inventory_index);

    if (changed)
    {
        // The activated row's own contents just changed size (an equipped
        // item swaps into/out of the inventory list) -- clamp back onto a
        // valid row rather than leaving the cursor past the end.
        const int total = RowCount(context);
        if (m_focused_index >= total)
            m_focused_index = total > 0 ? total - 1 : 0;
    }

    PublishSnapshot(context);
}

void CharacterScreenState::PublishSnapshot(GameplayContext& context)
{
    context.message_bus.Publish(
        BuildCharacterScreenMessage(context.registry, context.player, *m_affixes, *m_leveling, CurrentFocus()));
}

} // namespace psr
