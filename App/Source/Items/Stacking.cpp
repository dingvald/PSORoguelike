#include "Items/Stacking.h"

#include "Engine/ECS/ItemComponent.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"

#include <algorithm>

namespace psr {

bool MergeIntoMatchingStacks(Registry& registry, entt::entity incoming, const std::vector<entt::entity>& slots,
                              bool stop_after_first_match)
{
    ItemComponent* incoming_item = registry.TryGetComponent<ItemComponent>(incoming);
    if (!incoming_item || incoming_item->max_stack <= 1)
        return false;

    const PrefabIdComponent* incoming_prefab = registry.TryGetComponent<PrefabIdComponent>(incoming);
    if (!incoming_prefab)
        return false;

    bool matched = false;

    for (entt::entity slot : slots)
    {
        if (slot == incoming)
            continue;

        const PrefabIdComponent* slot_prefab = registry.TryGetComponent<PrefabIdComponent>(slot);
        if (!slot_prefab || slot_prefab->value != incoming_prefab->value)
            continue;

        ItemComponent* slot_item = registry.TryGetComponent<ItemComponent>(slot);
        if (!slot_item)
            continue;

        matched = true;

        const int room = slot_item->max_stack - slot_item->quantity;
        const int transfer = std::min(room, incoming_item->quantity);
        slot_item->quantity += transfer;
        incoming_item->quantity -= transfer;

        if (stop_after_first_match || incoming_item->quantity == 0)
            break;
    }

    return matched;
}

} // namespace psr
