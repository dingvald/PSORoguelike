#include "Items/Hotbar.h"

#include "Components/ConsumableComponent.h"
#include "Components/HotbarComponent.h"
#include "Components/InventoryComponent.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"

#include <cstddef>

namespace psr {

bool AssignItemToHotbarSlot(Entity actor, int inventory_index, int hotbar_slot)
{
    if (hotbar_slot < 0 || hotbar_slot >= static_cast<int>(HotbarComponent::kSlotCount))
        return false;

    InventoryComponent* inventory = actor.TryGet<InventoryComponent>();
    if (!inventory || inventory_index < 0 || inventory_index >= static_cast<int>(inventory->items.size()))
        return false;

    Registry& registry = actor.GetRegistry();
    const entt::entity item = inventory->items[static_cast<std::size_t>(inventory_index)];
    if (!registry.HasComponent<ConsumableComponent>(item))
        return false;
    const PrefabIdComponent* prefab_id = registry.TryGetComponent<PrefabIdComponent>(item);
    if (!prefab_id)
        return false;

    HotbarComponent& hotbar = actor.GetOrEmplace<HotbarComponent>();
    hotbar.slots[static_cast<std::size_t>(hotbar_slot)] = HotbarSlot{HotbarSlotType::Item, prefab_id->value};
    return true;
}

} // namespace psr
