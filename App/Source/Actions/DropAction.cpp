#include "Actions/DropAction.h"

#include "Combat/ActionCost.h"
#include "Components/InventoryComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Items/ItemDropEvent.h"

#include <cstddef>

namespace psr {

DropAction::DropAction(Grid& grid, int inventory_index) : m_grid(&grid), m_inventory_index(inventory_index) {}

ActionResult DropAction::Perform(Entity actor)
{
    InventoryComponent* inventory = actor.TryGet<InventoryComponent>();
    if (!inventory || m_inventory_index < 0 || m_inventory_index >= static_cast<int>(inventory->items.size()))
        return ActionResult(0);

    const entt::entity item = inventory->items[static_cast<std::size_t>(m_inventory_index)];
    inventory->items.erase(inventory->items.begin() + m_inventory_index);

    Registry& registry = actor.GetRegistry();
    const Vec2 tile = actor.Get<Position>().tile;
    registry.GetComponent<Position>(item).tile = tile;
    m_grid->AddEntity(tile, item);

    std::uint32_t item_prefab_id = 0;
    if (const PrefabIdComponent* prefab_id = registry.TryGetComponent<PrefabIdComponent>(item))
        item_prefab_id = prefab_id->value;

    AfterItemDropEvent event{item_prefab_id};
    actor.Dispatch(event);

    return ActionResult(EffectiveActCost(actor, kDropCost));
}

} // namespace psr
