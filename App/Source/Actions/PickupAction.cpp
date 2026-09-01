#include "Actions/PickupAction.h"

#include "Components/InventoryComponent.h"
#include "Engine/ECS/ItemComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Items/ItemPickupEvent.h"

#include <vector>

namespace psr {

PickupAction::PickupAction(Grid& grid) : m_grid(&grid) {}

ActionResult PickupAction::Perform(Entity actor)
{
    const Vec2 tile = actor.Get<Position>().tile;

    // Copied, not a reference -- RemoveEntity below mutates the very vector
    // GetEntities returns a reference to.
    const std::vector<entt::entity> occupants = m_grid->GetEntities(tile);

    Registry& registry = actor.GetRegistry();
    InventoryComponent& inventory = actor.GetOrEmplace<InventoryComponent>();

    bool picked_up_any = false;
    for (entt::entity occupant : occupants)
    {
        if (!registry.HasComponent<ItemComponent>(occupant))
            continue;
        if (static_cast<int>(inventory.items.size()) >= inventory.capacity)
            break;

        m_grid->RemoveEntity(tile, occupant);
        inventory.items.push_back(occupant);
        picked_up_any = true;

        std::uint32_t item_prefab_id = 0;
        if (const PrefabIdComponent* prefab_id = registry.TryGetComponent<PrefabIdComponent>(occupant))
            item_prefab_id = prefab_id->value;

        AfterItemPickupEvent event{item_prefab_id};
        actor.Dispatch(event);
    }

    return ActionResult(picked_up_any ? kPickupCost : 0);
}

} // namespace psr
