#include "Items/CharacterScreenSnapshot.h"

#include "Components/EquipmentComponent.h"
#include "Components/InventoryComponent.h"
#include "Engine/ECS/Registry.h"
#include "Items/ItemDisplayName.h"
#include "Messages/CharacterScreenMessage.h"

#include <array>
#include <cstddef>

namespace psr {

CharacterScreenMessage BuildCharacterScreenMessage(const Registry& registry, entt::entity player,
                                                   const AffixLibrary& affixes)
{
    CharacterScreenMessage message;

    if (const InventoryComponent* inventory = registry.TryGetComponent<InventoryComponent>(player))
    {
        message.inventory.reserve(inventory->items.size());
        for (entt::entity item : inventory->items)
            message.inventory.push_back(
                CharacterScreenMessage::ItemEntry{FormatItemDisplayName(registry, item, affixes)});
    }

    if (const EquipmentComponent* equipment = registry.TryGetComponent<EquipmentComponent>(player))
    {
        const std::array<entt::entity, 5> slots = {equipment->weapon, equipment->head, equipment->torso,
                                                   equipment->hands, equipment->legs};
        for (std::size_t i = 0; i < slots.size(); ++i)
        {
            if (slots[i] != entt::null)
                message.equipment[i] =
                    CharacterScreenMessage::ItemEntry{FormatItemDisplayName(registry, slots[i], affixes)};
        }
    }

    return message;
}

} // namespace psr
